#include "pressio_search.h"


struct fraz_search: public pressio_search_plugin {
  public:
    pressio_search_results search(std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)>) override {
      pressio_search_results results;
      return results;
    }

    //configuration
    virtual pressio_options get_options(pressio_options const& opt_module_settings) const override {
      return pressio_options{};
    }
    virtual int set_options(pressio_options const& options) override {
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    virtual const char* prefix() const override {
      return "fraz";
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

private:
};


static pressio_register X(search_plugins(), "fraz", [](){ return compat::make_unique<fraz_search>();});
