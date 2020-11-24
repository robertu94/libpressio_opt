#include "pressio_search.h"
#include "pressio_search_defines.h"
#include "pressio_search_results.h"
#include <algorithm>
#include <chrono>
#include <iterator>
#include <libdistributed_work_queue.h>
#include <limits>
#include <random>
#include <time.h>
#include <mpi.h>
#include <std_compat/memory.h>
#include <libpressio_ext/cpp/distributed_manager.h>

namespace {
auto
loss(pressio_search_results::output_type::value_type target,
     pressio_search_results::output_type::value_type actual)
{
  return std::abs(target - actual);
}
} // namespace

struct random_search : public pressio_search_plugin
{
private:
  using task_request_t = std::tuple<std::vector<double>>; // input
  using task_response_t =
    std::tuple<std::vector<double>, std::vector<double>>; // input, output

public:
  pressio_search_results search(
    std::function<pressio_search_results::output_type(
      pressio_search_results::input_type const&)>
      compress_fn,
    distributed::queue::StopToken& token) override
  {
    pressio_search_results best_results{};
    double best_objective;
    switch (mode) {
      case pressio_search_mode_max:
        best_objective = std::numeric_limits<
          pressio_search_results::output_type::value_type>::lowest();
        break;
      case pressio_search_mode_min:
        best_objective = std::numeric_limits<
          pressio_search_results::output_type::value_type>::max();
        break;
      default:
      case pressio_search_mode_target:
        best_objective = std::numeric_limits<
          pressio_search_results::output_type::value_type>::max();
        break;
    }
    if (max_iterations < 1) {
      best_results.status = -2;
      best_results.msg = "at least 1 iterations are required";
      return best_results;
    }

    std::seed_seq seed_s {seed.value_or(time(nullptr))};
    std::default_random_engine gen{seed_s};

    auto point_generator = [this, &gen]() {
      using value_type = pressio_search_results::input_type::value_type;
      pressio_search_results::input_type input(this->lower_bound.size());
      std::transform(std::begin(this->lower_bound), std::end(this->lower_bound),
                     std::begin(this->upper_bound), std::begin(input),
                     [&gen](value_type lower, value_type upper) {
                       std::uniform_real_distribution<value_type> dist(lower,
                                                                       upper);
                       return dist(gen);
                     });
      return task_request_t{ std::move(input) };
    };

    std::vector<task_request_t> inital_points;
    inital_points.reserve(max_iterations);
    std::generate_n(std::back_inserter(inital_points), max_iterations,
                    point_generator);

    auto start_time = std::chrono::system_clock::now();
    auto should_stop = [this, &token, start_time]() {
      auto current_time = std::chrono::system_clock::now();
      return token.stop_requested() ||
             std::chrono::duration_cast<std::chrono::seconds>(current_time -
                                                              start_time)
                 .count() > max_seconds;
    };

    manager.work_queue(
      std::begin(inital_points), std::end(inital_points),
      [&compress_fn](task_request_t const& request) {
        auto const& inputs = std::get<0>(request);
        pressio_search_results::output_type result = compress_fn(inputs);
        return task_response_t{ inputs, result };
      },
      [&best_results, &best_objective, &token, &should_stop,
       this](task_response_t response,
             distributed::queue::TaskManager<task_request_t, MPI_Comm>& task_manager) {
        const auto& inputs = std::get<0>(response);
        const auto& objective = std::get<1>(response).front();

        switch (mode) {
          case pressio_search_mode_max:
            if (objective > best_objective) {
              best_objective = objective;
              best_results.output = std::get<1>(response);
              best_results.inputs = inputs;
              if(target && objective > *target) {
                task_manager.request_stop();
                token.request_stop();
              }
            }
            break;
          case pressio_search_mode_min:
            if (objective < best_objective) {
              best_objective = objective;
              best_results.output = std::get<1>(response);
              best_results.inputs = inputs;
              if(target && objective < *target) {
                task_manager.request_stop();
                token.request_stop();
              }
            }
            break;
          case pressio_search_mode_target:
            if (loss(*target, objective) < best_objective) {
              best_results.output = std::get<1>(response);
              best_results.inputs = inputs;
              best_objective = loss(*target, objective);
              if (best_objective <
                    loss(*target * (1.0 + global_rel_tolerance), *target) ||
                  best_objective <
                    loss(*target * (1.0 - global_rel_tolerance), *target)) {
                token.request_stop();
                task_manager.request_stop();
              }
            }
            break;
        }

        if (should_stop()) {
          task_manager.request_stop();
        }
      });

    manager.bcast(best_results.inputs);
    manager.bcast(best_results.output);
    manager.bcast(best_results.status);
    manager.bcast(best_results.msg);
    return best_results;
  }

  // configuration
  pressio_options get_options() const override
  {
    pressio_options opts;

    // need to reconfigure because input size has changed
    set(opts, "opt:prediction", pressio_data(std::begin(prediction), std::end(prediction)));
    set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(lower_bound)));
    set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));
    set(opts, "opt:max_iterations", max_iterations);
    set(opts, "opt:max_seconds", max_seconds);
    set(opts, "opt:target", target);
    set(opts, "opt:objective_mode", mode);
    opts.copy_from(manager.get_options());
    set(opts,"random:seed", seed);
    return opts;
  }
  int set_options(pressio_options const& options) override
  {
    pressio_data data;
    if (options.get("opt:lower_bound", &data) == pressio_options_key_set) {
      lower_bound =
        data.to_vector<pressio_search_results::input_element_type>();
    }
    if (options.get("opt:upper_bound", &data) == pressio_options_key_set) {
      upper_bound =
        data.to_vector<pressio_search_results::input_element_type>();
    }
    options.get("opt:max_iterations", &max_iterations);
    options.get("opt:max_seconds", &max_seconds);
    options.get("opt:target", &target);
    options.get("opt:objective_mode", &mode);
    manager.set_options(options);
    options.get("random:seed", &seed);
    return 0;
  }

  // meta-data
  /** get the prefix used by this compressor for options */
  const char* prefix() const override { return "random"; }

  /** get a version string for the compressor
   * \see pressio_compressor_version for the semantics this function should obey
   */
  const char* version() const override { return "0.0.1"; }
  /** get the major version, default version returns 0
   * \see pressio_compressor_major_version for the semantics this function
   * should obey
   */
  int major_version() const override { return 0; }
  /** get the minor version, default version returns 0
   * \see pressio_compressor_minor_version for the semantics this function
   * should obey
   */
  int minor_version() const override { return 0; }
  /** get the patch version, default version returns 0
   * \see pressio_compressor_patch_version for the semantics this function
   * should obey
   */
  int patch_version() const override { return 1; }

  std::shared_ptr<pressio_search_plugin> clone() override
  {
    return compat::make_unique<random_search>(*this);
  }

private:
  pressio_search_results::input_type prediction;
  pressio_search_results::input_type lower_bound;
  pressio_search_results::input_type upper_bound;
  compat::optional<pressio_search_results::output_type::value_type> target;
  unsigned int max_iterations = 100;
  unsigned int max_seconds = std::numeric_limits<unsigned int>::max();
  unsigned int mode = pressio_search_mode_none;
  compat::optional<unsigned int> seed;
  pressio_distributed_manager manager = pressio_distributed_manager(
      /*max_masters*/1,
      /*max_ranks_per_worker*/1
      );
  double global_rel_tolerance = .1;
};

static pressio_register guess_random_register(search_plugins(), "random_search", []() {
  return compat::make_unique<random_search>();
});
