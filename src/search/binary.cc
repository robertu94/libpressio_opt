#include <algorithm>
#include <chrono>
#include "pressio_search.h"
#include "libpressio_opt_ext/impl/pressio_data_utilities.h"
#include <libpressio_ext/compat/std_compat.h>

struct binary_search: public pressio_search_plugin {
  public:
    pressio_search_results search(
        std::function<pressio_search_results::objective_type(pressio_search_results::input_type const&)> compress_fn,
        distributed::queue::StopToken& token
        ) override {
      pressio_search_results results;
      size_t iter = 2;
      auto lower = lower_bound.front();
      auto upper = upper_bound.front();
      auto current = (upper-lower)/2.0 + lower;
      compat::optional<decltype(lower)> lower_value{};
      compat::optional<decltype(upper)> upper_value{};
      auto last_time = std::chrono::system_clock::now();
      auto max_time = std::chrono::system_clock::now() + std::chrono::seconds(max_seconds);
      auto result = compress_fn({current});

      auto is_nonmonotonic = [&]() {
        return  (lower_value && *lower_value > result) || //check for non-monotonicity on the lower bound
                (upper_value && *upper_value < result); //check for non-monotonicity on the upper bound
      };
      auto check_global_tolerance = [&]{
        return ((1.0-global_rel_tolerance)*target <= result && result <= (1.0+global_rel_tolerance)*target);
      };
      auto is_done = [&](){
        return 
          check_global_tolerance() || //check global tolerance
          (iter > max_iterations) || //exceeded maximum iterations
          (max_seconds > 0 && (last_time = std::chrono::system_clock::now()) > max_time) || //check for time exceeded
          (lower > upper) || //check for floating point rounding errors
          is_nonmonotonic() || //check for non-monotonic results, violation of assumptions
          token.stop_requested()
          ;
      };
      while(not is_done()) {
        if(result < target) {
          lower = current;
          lower_value = result;
        } else {
          upper = current;
          upper_value = result;
        }

        current = (upper-lower)/2.0 + lower;
        result = compress_fn({current});
        ++iter;
      }
      if(check_global_tolerance()) {
        token.request_stop();
      }
      if(is_nonmonotonic()) {
        results.status = 1;
        results.msg = "binary search objective function was non-monotonic, violation of assumptions";
      }
      if(iter > max_iterations) {
        results.status = -1;
        results.msg = "iterations exceeded";
      }
      if(last_time > max_time) {
        results.status = -2;
        results.msg = "time-limit exceeded";
      }
      results.inputs = {current};
      results.objective = result;

      return results;
    }

    //configuration
    virtual pressio_options get_options(pressio_options const& opt_module_settings) const override {
      pressio_options opts;
      std::vector<std::string> inputs;
      opt_module_settings.get("opt:inputs", &inputs);
      
      //need to reconfigure because input size has changed
      if(inputs.size() != prediction.size()) {
        opts.set("opt:prediction",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
        opts.set("opt:lower_bound",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
        opts.set("opt:upper_bound",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
      } else {
        opts.set("opt:prediction", vector_to_owning_pressio_data(prediction));
        opts.set("opt:lower_bound", vector_to_owning_pressio_data(lower_bound));
        opts.set("opt:upper_bound", vector_to_owning_pressio_data(upper_bound));
      }
      opts.set("opt:max_iterations", max_iterations);
      opts.set("opt:max_seconds", max_seconds);
      opts.set("opt:global_rel_tolerance", global_rel_tolerance);
      opts.set("opt:target", target);
      return opts;
    }
    virtual int set_options(pressio_options const& options) override {
      pressio_data data;
      if(options.get("opt:prediction", &data) == pressio_options_key_set) {
        prediction = pressio_data_to_vector<pressio_search_results::input_element_type>(data);
        if(prediction.size() > 1) return 1;
      }
      if(options.get("opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = pressio_data_to_vector<pressio_search_results::input_element_type>(data);
        if(lower_bound.size() > 1) return 1;
      }
      if(options.get("opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = pressio_data_to_vector<pressio_search_results::input_element_type>(data);
        if(upper_bound.size() > 1) return 1;
      }
      options.get("opt:max_iterations", &max_iterations);
      options.get("opt:max_seconds", &max_seconds);
      options.get("opt:global_rel_tolerance", &global_rel_tolerance);
      options.get("opt:target", &target);
      return 0;
    }
    virtual int check_options(pressio_options const& options) const override {
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    virtual const char* prefix() const override {
      return "binary";
    }

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    virtual const char* version() const override {
      return "0.0.2";
    }
    /** get the major version, default version returns 0
     * \see pressio_compressor_major_version for the semantics this function should obey
     */
    virtual int major_version() const override { return 0; }
    /** get the minor version, default version returns 0
     * \see pressio_compressor_minor_version for the semantics this function should obey
     */
    virtual int minor_version() const override { return 0; }
    /** get the patch version, default version returns 0
     * \see pressio_compressor_patch_version for the semantics this function should obey
     */
    virtual int patch_version() const override { return 2; }

    std::shared_ptr<pressio_search_plugin> clone() override {
      return compat::make_unique<binary_search>(*this);
    }

private:
    pressio_search_results::input_type prediction;
    pressio_search_results::input_type lower_bound;
    pressio_search_results::input_type upper_bound;
    pressio_search_results::objective_type target;
    double global_rel_tolerance;
    unsigned int max_iterations;
    unsigned int max_seconds;
};


static pressio_register X(search_plugins(), "binary", [](){ return compat::make_unique<binary_search>();});
