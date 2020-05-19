#include "pressio_search.h"
#include "pressio_search_results.h"
#include "pressio_search_defines.h"

struct guess_first_search: public pressio_search_plugin {
  public:
    guess_first_search() {
      search_method = search_plugins().build(search_method_str);
    }

    pressio_search_results search(
        std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn,
        distributed::queue::StopToken& stop_token
        ) override {
      pressio_search_results results{};
      results.inputs = input;
      results.output = compress_fn(input);
      switch(mode) {
        case pressio_search_mode_target:
          {
            if(results.output.front() < *target * (1+ global_rel_tolerance) && 
               results.output.front() > *target * (1- global_rel_tolerance)) {
              stop_token.request_stop();
              return results;
            }
          }
        case pressio_search_mode_min:
          {
            if(target && results.output.front() < *target) {
              stop_token.request_stop();
              return results;
            }
          }
        case pressio_search_mode_max:
          {
            if(target && results.output.front() > *target) {
              stop_token.request_stop();
              return results;
            }
          }
      }
      return search_method->search(compress_fn, stop_token);
    }

    //configuration
    pressio_options get_options(pressio_options const& opt_module_settings) const override {
      pressio_options opts;
      std::vector<std::string> inputs;
      opt_module_settings.get("opt:inputs", &inputs);
      
      //need to reconfigure because input size has changed
      if(inputs.size() != input.size()) {
        set(opts, "opt:prediction",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
      } else {
        set(opts, "opt:prediction", pressio_data(std::begin(input), std::end(input)));
      }
      set(opts, "opt:target", target);
      set(opts, "opt:objective_mode", mode);
      set(opts, "opt:global_rel_tolerance", global_rel_tolerance);
      set(opts, "guess_if:search", search_method_str);
      return opts;
    }

    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(get(options, "opt:prediction", &data) == pressio_options_key_set) {
        input = data.to_vector<pressio_search_results::input_element_type>();
      }
      get(options, "opt:target", &target);
      get(options, "opt:objective_mode", &mode);
      get(options, "guess_if:search", &search_method_str);
      return 0;
    }

    void set_name_impl(std::string const& new_name) override {
      search_method->set_name(new_name + "/" + search_method->prefix());
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "guess_if";
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
      return compat::make_unique<guess_first_search>(*this);
    }

private:
    pressio_search_results::input_type input;
    compat::optional<pressio_search_results::output_type::value_type> target;
    unsigned int mode = pressio_search_mode_target;
    double global_rel_tolerance = 0.0;
    std::string search_method_str = "guess";
    pressio_search search_method;
};


static pressio_register X(search_plugins(), "guess_first", [](){ return compat::make_unique<guess_first_search>();});
