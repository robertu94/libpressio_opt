#include <algorithm>
#include <limits>
#include "impl/GlobalOptimization.h"
#include "pressio_search.h"
#include "pressio_search_defines.h"
#include "pressio_search_results.h"
#include <libpressio_ext/compat/memory.h>

namespace {
    auto clamp(double value, double low, double high) {
      assert(low < high);
      if(value < low) return low;
      if(high < value) return high;
      return value;
    };
    auto loss(double target, double actual){
      return clamp(pow((target-actual),2),
          std::numeric_limits<double>::min() * 1e-10,
          std::numeric_limits<double>::max() * 1e-10
          );
    };
    auto vector_to_dlib(pressio_search_results::input_type const& input) {
      dlib::matrix<double,0,1> output(input.size());
      std::copy(std::begin(input), std::end(input), std::begin(output));

      return output;
    };
    auto dlib_to_vector(dlib::matrix<double,0,1> const& input) {
      pressio_search_results::input_type output(input.begin(), input.end());
      return output;
    };
}

struct fraz_search: public pressio_search_plugin {
  public:
    pressio_search_results search(
        std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn,
        distributed::queue::StopToken& token
        ) override {

      pressio_search_results results;
      dlib::function_evaluation best_result;
      std::map<pressio_search_results::input_type, pressio_search_results::output_type> cache;
      dlib::thread_pool pool((thread_safe) ? (nthreads): (1));

      switch(mode) {
        case pressio_search_mode_target:
          {
            auto threshold = std::min(
                loss(*target, *target*(1-global_rel_tolerance)),
                loss(*target, *target*(1+global_rel_tolerance))
            );
            auto should_stop = [threshold,&token](double value) {
              bool target_achived = value < threshold;
              if (target_achived) token.request_stop();
              return target_achived || token.stop_requested();
            };

            auto fraz = [&cache, &compress_fn, this](dlib::matrix<double,0,1> const& input){
              auto const vec = dlib_to_vector(input);
              auto const result = compress_fn(vec);
              cache[vec] = result;
              return loss(*target, result.front());
            };

            best_result = pressio_opt::find_min_global(
                pool,
                fraz,
                vector_to_dlib(lower_bound),
                vector_to_dlib(upper_bound),
                pressio_opt::max_function_calls(max_iterations),
                std::chrono::seconds(max_seconds),
                local_tolerance,
                pressio_opt::stop_condition(should_stop)
                );
            break;
          }
        case pressio_search_mode_max:
        case pressio_search_mode_min:
          {
            auto fraz = [&cache, &compress_fn, this](dlib::matrix<double,0,1> const& input){
              auto const vec = dlib_to_vector(input);
              auto const result = compress_fn(vec);
              cache[vec] = result;
              return clamp(result.front(),
                std::numeric_limits<double>::min() * 1e-10,
                std::numeric_limits<double>::max() * 1e-10
              );
            };
            if(mode == pressio_search_mode_min) {
            auto should_stop = [&token, this](double value) {
              bool target_achived = (target && value < *target);
              if (target_achived) token.request_stop();
              return token.stop_requested() || target_achived;
            };
            best_result = pressio_opt::find_min_global(
                pool,
                fraz,
                vector_to_dlib(lower_bound),
                vector_to_dlib(upper_bound),
                pressio_opt::max_function_calls(max_iterations),
                std::chrono::seconds(max_seconds),
                local_tolerance,
                pressio_opt::stop_condition(should_stop)
                );
            } else {
            auto should_stop = [&token, this](double value) {
              bool target_achived = (target && value > *target);
              if (target_achived) token.request_stop();
              return token.stop_requested() || target_achived;
            };
            best_result = pressio_opt::find_max_global(
                pool,
                fraz,
                vector_to_dlib(lower_bound),
                vector_to_dlib(upper_bound),
                pressio_opt::max_function_calls(max_iterations),
                std::chrono::seconds(max_seconds),
                local_tolerance,
                pressio_opt::stop_condition(should_stop)
                );
            }
            break;
          }
      }

      results.inputs = dlib_to_vector(best_result.x);
      results.output = cache[results.inputs];
      results.status = 0;

      return results;
    }

    //configuration
    pressio_options get_options(pressio_options const& opt_module_settings) const override {
      pressio_options opts;

      //need to reconfigure because input size has changed
      set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(lower_bound)));
      set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));
      set(opts, "opt:max_iterations", max_iterations);
      set(opts, "opt:max_seconds", max_seconds);
      set(opts, "opt:global_rel_tolerance", global_rel_tolerance);
      set(opts, "opt:local_rel_tolerance", local_tolerance);
      set(opts, "opt:target", target);
      set(opts, "opt:objective_mode", mode);
      set(opts, "fraz:nthreads", nthreads);
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
      get(options, "opt:max_iterations", &max_iterations);
      get(options, "opt:max_seconds", &max_seconds);
      get(options, "opt:global_rel_tolerance", &global_rel_tolerance);
      get(options, "opt:local_rel_tolerance", &local_tolerance);
      get(options, "opt:target", &target);
      get(options, "opt:thread_safe", &thread_safe);
      get(options, "fraz:nthreads", &nthreads);
      get(options, "opt:objective_mode", &mode);

      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "fraz";
    }

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    const char* version() const override {
      return "0.0.2";
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
    int patch_version() const override { return 2; }

    std::shared_ptr<pressio_search_plugin> clone() override {
      return compat::make_unique<fraz_search>(*this);
    }

private:
    pressio_search_results::input_type lower_bound{};
    pressio_search_results::input_type upper_bound{};
    compat::optional<pressio_search_results::output_type::value_type> target{};
    double local_tolerance = .01;
    double global_rel_tolerance = .1;
    unsigned int max_iterations = 100;
    unsigned int max_seconds = std::numeric_limits<unsigned int>::max();
    unsigned int mode = pressio_search_mode_target;
    unsigned int nthreads = 1;
    int thread_safe = 0;
};


static pressio_register fraz_register(search_plugins(), "fraz", [](){ return compat::make_unique<fraz_search>();});
