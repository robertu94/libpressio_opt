#include "pressio_search.h"
#include "pressio_search_results.h"
#include "pressio_search_defines.h"
#include <cmath>
#include <libdistributed_work_queue.h>
#include <std_compat/memory.h>
#include <libpressio_ext/cpp/distributed_manager.h>

namespace {
  auto loss(pressio_search_results::output_type::value_type target, pressio_search_results::output_type::value_type actual) {
    return fabs(target-actual);
  }
}

struct dist_gridsearch_search: public pressio_search_plugin {
  public:

    dist_gridsearch_search() {
      search_method = search_plugins().build(search_method_str);
    }

    pressio_search_results search(compat::span<const pressio_data *const> const &input_datas,
                                  std::function<pressio_search_results::output_type(
                                          pressio_search_results::input_type const &)> compress_fn,
                                  distributed::queue::StopToken &stop_token) override {



      pressio_search_results best_results;
      pressio_search_results::output_type::value_type best_objective;
      switch(mode){
        case pressio_search_mode_max:
          best_objective = std::numeric_limits<
            pressio_search_results::output_type::value_type>::lowest();
          break;
        case pressio_search_mode_min:
          best_objective =
            std::numeric_limits<pressio_search_results::output_type::value_type>::max();
          break;
        case pressio_search_mode_target:
          best_objective =
            std::numeric_limits<pressio_search_results::output_type::value_type>::max();
          break;
        default:
          best_objective = 0;
          break;
      }

      if(num_bins.size() == 0) {
        best_results.status = 1;
        best_results.msg = "dist_gridsearch was not configured with non-empty bin sizes";
        return best_results;
      } else if(lower_bound.size() != upper_bound.size() || lower_bound.size() != num_bins.size()) {
        best_results.status = 1;
        best_results.msg = "dist_gridsearch was configured with lower_bounds, upper_bounds, or num_bins of different sizes";
        return best_results;
      }

      auto tasks = build_task_list();

      manager.
        work_queue(
          std::begin(tasks), std::end(tasks),
          [this, &input_datas,compress_fn](
            task_request_t const& task,
            distributed::queue::TaskManager<task_request_t, MPI_Comm>& task_manager) {
            //set lower and upper bounds
            auto grid_lower = std::get<0>(task);
            auto grid_upper = std::get<1>(task);

            pressio_options options;
            options.set("opt:lower_bound", pressio_data(std::begin(grid_lower), std::end(grid_lower)));
            options.set("opt:upper_bound", pressio_data(std::begin(grid_upper), std::end(grid_upper)));
            options.set("distributed:comm", (void*)task_manager.get_subcommunicator());
            search_method->set_options(options);

            if(task_manager.stop_requested()) {
              return task_response_t{std::vector<double>{}, 1, std::vector<double>{}};
            } else {
              auto grid_result = search_method->search(input_datas, compress_fn, task_manager);
              return task_response_t{grid_result.output, grid_result.status, grid_result.inputs};
            }
          },
          [this, &best_results,&best_objective,&stop_token](task_response_t response,
            distributed::queue::TaskManager<task_request_t, MPI_Comm>& task_manager
            ) {
            auto const& status = std::get<1>(response);
            auto const& inputs = std::get<2>(response);
            if(status == 0 && std::get<0>(response).size() >= 1) {
              auto const& actual = std::get<0>(response).front();
              switch(mode){
                case pressio_search_mode_max:
                  if(best_objective < actual) {
                    best_objective = actual;
                    best_results.output = std::get<0>(response);
                    best_results.inputs = inputs;
                    if(target && actual > *target){
                      stop_token.request_stop();
                      task_manager.request_stop();
                    }
                  }
                  break;
                case pressio_search_mode_min:
                  if(best_objective > actual) {
                    best_objective = actual;
                    best_results.output = std::get<0>(response);
                    best_results.inputs = inputs;
                    if(target && actual < *target) {
                      stop_token.request_stop();
                      task_manager.request_stop();
                    }
                  }
                  break;
                case pressio_search_mode_target:
                  if (loss(*target, actual) < best_objective) {
                    best_results.output = std::get<0>(response);
                    best_results.inputs = inputs;
                    best_objective = loss(*target, actual);
                    if(best_objective < loss(*target*(1.0+global_rel_tolerance), *target) ||
                       best_objective < loss(*target*(1.0-global_rel_tolerance), *target)) {
                      stop_token.request_stop();
                      task_manager.request_stop();
                    }
                  }
                  break;
              }
            }
            if(stop_token.stop_requested()) {
              task_manager.request_stop();
            }
          });
      manager.bcast(best_results.inputs);
      manager.bcast(best_results.output);
      manager.bcast(best_results.status);
      manager.bcast(best_results.msg);
      return best_results;
    }

    //configuration
    pressio_options get_options() const override {
      pressio_options opts;
      
      //need to reconfigure because input size has changed
      set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(lower_bound)));
      set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));
      set(opts, "dist_gridsearch:num_bins", pressio_data(std::begin(num_bins), std::end(num_bins)));
      set(opts, "dist_gridsearch:overlap_percentage", pressio_data(std::begin(overlap_percentage), std::end(overlap_percentage)));
      set(opts, "opt:target", target);
      set(opts, "opt:global_rel_tolerance", global_rel_tolerance);
      opts.copy_from(manager.get_options());
      set_meta(opts, "dist_gridsearch:search", search_method_str, search_method);
      set(opts, "opt:objective_mode", mode);

      return opts;
    }

    pressio_options get_configuration_impl() const override {
        pressio_options opts;
        set_meta_configuration(opts, "dist_gridsearch:search", search_plugins(), search_method);
        return opts;
    }

    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(get(options, "opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      if(get(options, "opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      if(get(options, "dist_gridsearch:num_bins", &data) == pressio_options_key_set) {
        num_bins = data.to_vector<size_t>();
      }
      if(get(options, "dist_gridsearch:overlap_percentage", &data) == pressio_options_key_set) {
        overlap_percentage = data.to_vector<double>();
      }
      get(options, "opt:target", &target);
      manager.set_options(options);
      get(options, "opt:global_rel_tolerance", &global_rel_tolerance);
      get(options, "opt:objective_mode", &mode);
      get_meta(options, "dist_gridsearch:search", search_plugins(), search_method_str, search_method);
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "dist_gridsearch";
    }

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    const char* version() const override {
      return "0.0.1";
    }
    /** get the major version, default version returns 0
     * \see pressio_compressor_major_version for the semantics this function should obey
     */
    int major_version() const override { return 0; }
    /** get the minor version, default version returns 0
     * \see pressio_compressor_minor_version for the semantics this function should obey
     */
    int minor_version() const override { return 0; }
    /** get the patch version, default version returns 0
     * \see pressio_compressor_patch_version for the semantics this function should obey
     */
    int patch_version() const override { return 1; }

    std::shared_ptr<pressio_search_plugin> clone() override {
      return compat::make_unique<dist_gridsearch_search>(*this);
    }

    void set_name_impl(std::string const& new_name) override {
      search_method->set_name(new_name + "/" + search_method->prefix());
      manager.set_name(new_name);
    }

private:
    using task_request_t = std::tuple<std::vector<double>, std::vector<double>>; //<0>=lower_bound, <1>=upper_bound
    using task_response_t  = std::tuple<std::vector<double>, int, std::vector<double>>; //<0> multi-objective <1> status <2> best_input


    std::vector<task_request_t> build_task_list() {
      std::vector<task_request_t> tasks;
      std::vector<double> step(lower_bound.size());
      std::vector<double> overlap(lower_bound.size());
      for (size_t dim = 0; dim < lower_bound.size(); ++dim) {
        step[dim] = (upper_bound[dim] - lower_bound[dim]) / static_cast<double>(num_bins[dim]);
        overlap[dim] = overlap_percentage[dim] * step[dim];
      }

      //be sure to include overlap_percentage % overlap between the bins
      //to ensure sufficient stationary points at the end point

      std::vector<size_t> bin(lower_bound.size(), 0);
      bool done = false;
      size_t idx = 0;
      while(!done) {

        std::vector<double> grid_lower(lower_bound.size());
        std::vector<double> grid_upper(lower_bound.size());
        for (size_t dim = 0; dim < lower_bound.size(); ++dim) {
          grid_lower[dim] = std::max(lower_bound[dim], lower_bound[dim] + step[dim] * bin[dim] - overlap[dim]);
          grid_upper[dim] = std::min(upper_bound[dim], lower_bound[dim] + step[dim] * static_cast<double>(bin[dim] + 1) + overlap[dim]);
        }
        tasks.emplace_back(
            grid_lower,
            grid_upper
        );

        bool updating = true;
        while(updating) {
          ++bin[idx];
          if(bin[idx] == num_bins[idx]) {
            bin[idx] = 0;
            idx++;
            if(idx == num_bins.size()) {
              done = true;
              updating = false;
            } else {
              updating = true;
            }
          } else {
            updating = false;
          }
        }
        idx = 0;
      }
      return tasks;
    }

    std::vector<std::string> children() const final override {
        return {
            search_method->get_name()
        };
    }


    std::vector<double> overlap_percentage;
    pressio_search_results::input_type lower_bound{};
    pressio_search_results::input_type upper_bound{};
    std::vector<size_t> num_bins{};
    std::string search_method_str = "guess";
    pressio_search search_method;
    unsigned int mode = pressio_search_mode_none;
    compat::optional<pressio_search_results::output_type::value_type> target;
    double global_rel_tolerance = .1;
    pressio_distributed_manager manager = pressio_distributed_manager(
        /*max_ranks_per_worker*/pressio_distributed_manager::unlimited,
        /*max_masters*/1
        );
};


static pressio_register dist_search_register(search_plugins(), "dist_gridsearch", [](){ return compat::make_unique<dist_gridsearch_search>();});
