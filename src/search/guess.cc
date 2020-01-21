#include "pressio_search.h"
#include "pressio_search_results.h"
#include "libpressio_opt_ext/impl/pressio_data_utilities.h"

struct guess_search: public pressio_search_plugin {
  public:
    pressio_search_results search(std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn) override {
      pressio_search_results results{};
      results.inputs = input;
      results.output = compress_fn(input);
      return results;
    }

    //configuration
    virtual pressio_options get_options(pressio_options const& opt_module_settings) const override {
      pressio_options opts;
      std::vector<std::string> inputs;
      opt_module_settings.get("opt:inputs", &inputs);
      
      //need to reconfigure because input size has changed
      if(inputs.size() != input.size()) {
        opts.set("opt:prediction",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
      } else {
        opts.set("opt:prediction", vector_to_owning_pressio_data(input));
      }
      return opts;
    }

    virtual int set_options(pressio_options const& options) override {
      pressio_data data;
      if(options.get("opt:prediction", &data) == pressio_options_key_set) {
        input = pressio_data_to_vector<pressio_search_results::input_element_type>(data);
      }
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    virtual const char* prefix() const override {
      return "guess";
    }

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    virtual const char* version() const override {
      return "0.0.1";
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
    virtual int patch_version() const override { return 1; }

private:
    pressio_search_results::input_type input;
};


static pressio_register X(search_plugins(), "guess", [](){ return compat::make_unique<guess_search>();});
