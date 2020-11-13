#include <algorithm>
#include <chrono>
#include "pressio_search.h"
#include <libpressio_ext/compat/std_compat.h>

struct binary_search: public pressio_search_plugin {
  public:
    pressio_search_results search(
        std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn,
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
      auto result_v = compress_fn({current});
      auto result = result_v.front();

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
        result_v = compress_fn({current});
        result = result_v.front();
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
      results.output = result_v;

      return results;
    }

    //configuration
    virtual pressio_options get_options() const override {
      pressio_options opts;
      
      //need to reconfigure because input size has changed
      set(opts, "opt:prediction", pressio_data(std::begin(prediction), std::end(prediction)));
      set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(prediction)));
      set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));
      set(opts, "opt:max_iterations", max_iterations);
      set(opts, "opt:max_seconds", max_seconds);
      set(opts, "opt:global_rel_tolerance", global_rel_tolerance);
      set(opts, "opt:target", target);
      return opts;
    }
    virtual int set_options(pressio_options const& options) override {
      pressio_data data;
      if(get(options, "opt:prediction", &data) == pressio_options_key_set) {
        prediction = data.to_vector<pressio_search_results::input_element_type>();
        if(prediction.size() > 1) return 1;
      }
      if(get(options, "opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = data.to_vector<pressio_search_results::input_element_type>();
        if(lower_bound.size() > 1) return 1;
      }
      if(get(options, "opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = data.to_vector<pressio_search_results::input_element_type>();
        if(upper_bound.size() > 1) return 1;
      }
      get(options, "opt:max_iterations", &max_iterations);
      get(options, "opt:max_seconds", &max_seconds);
      get(options, "opt:global_rel_tolerance", &global_rel_tolerance);
      get(options, "opt:target", &target);
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
    pressio_search_results::output_type::value_type target;
    double global_rel_tolerance;
    unsigned int max_iterations;
    unsigned int max_seconds;
};


static pressio_register binary_search_register(search_plugins(), "binary", [](){ return compat::make_unique<binary_search>();});
