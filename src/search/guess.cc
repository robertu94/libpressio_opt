#include "pressio_search.h"
#include "pressio_search_results.h"
#include <libpressio_ext/compat/memory.h>

struct guess_search: public pressio_search_plugin {
  public:
    pressio_search_results search(
        std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn,
        distributed::queue::StopToken&
        ) override {
      pressio_search_results results{};
      results.inputs = input;
      results.output = compress_fn(input);
      return results;
    }

    //configuration
    pressio_options get_options() const override {
      pressio_options opts;
      
      //need to reconfigure because input size has changed
      set(opts, "opt:prediction", pressio_data(std::begin(input), std::end(input)));
      return opts;
    }

    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(options.get("opt:prediction", &data) == pressio_options_key_set) {
        input = data.to_vector<pressio_search_results::input_element_type>();
      }
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "guess";
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
      return compat::make_unique<guess_search>(*this);
    }

private:
    pressio_search_results::input_type input;
};


static pressio_register guess_register(search_plugins(), "guess", [](){ return compat::make_unique<guess_search>();});
