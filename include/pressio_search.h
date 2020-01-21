#include <functional>
#include <vector>
#include <libpressio_ext/cpp/pressio.h>
#include <libpressio_ext/cpp/options.h>
#include "pressio_search_results.h"

/**
 * \file
 * \brief interface for search modules
 */

/**
 * base class for search plugins
 */
struct pressio_search_plugin {
  public:
    /** destructor */
    virtual ~pressio_search_plugin()=default;

    /**
     * preform the actual search using some optimization routine
     *
     * \param[in] compress_fn - the function to call to compress the data and computes the desired metrics. It accepts
     *     multiple inputs the order specified by the "opt:input" option as
     *     doubles.  It returns the metric specified by the "opt:output" option
     *     as a double. It will call the appropriate hooks in pressio_search_metrics
     *
     * \returns a structure that summarizes the "best-configuration" found as determined by the module
     * \see pressio_search_metrics
     */
    virtual pressio_search_results search(std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn)=0;

    //configuration
    /**
     * get the runtime settings of the search module 
     *
     * \param[in] opt_module_settings the settings determined by the opt
     * compressor module, at minimum input (of type const char*[]) and output
     * (of type const char*)
     *
     * \returns the options for this search module
     */
    virtual pressio_options get_options(pressio_options const& opt_module_settings) const=0;
    /** apply the options passed the search module 
     * \param[in] options the configuration to apply
     * \returns 0 if there was no error, non zero if there was a error
     * */
    virtual int set_options(pressio_options const& options)=0;
    /**
     * get the compile-time settings of the search module 
     *
     * \returns the options for this search module
     */
    virtual pressio_options get_configuration() const { return pressio_options(); }
    /** validate the options passed the search module 
     * \param[in] options the configuration to check
     * \returns 0 if there was no error, non zero if there was a error
     * */
    virtual int check_options(pressio_options const& options) const {return 0;}

    //meta-data
    /** get the prefix used by this compressor for options */
    virtual const char* prefix() const=0;

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    virtual const char* version() const=0;
    /** get the major version, default version returns 0
     * \see pressio_compressor_major_version for the semantics this function should obey
     */
    virtual int major_version() const { return 0; }
    /** get the minor version, default version returns 0
     * \see pressio_compressor_minor_version for the semantics this function should obey
     */
    virtual int minor_version() const { return 0; }
    /** get the patch version, default version returns 0
     * \see pressio_compressor_patch_version for the semantics this function should obey
     */
    virtual int patch_version() const { return 0; }

private:
};

/**
 * wrapper interface for C usage if required
 */
struct pressio_search {
  /** constructor 
   * \param[in] impl the plugin to construct
   * */
  pressio_search(std::shared_ptr<pressio_search_plugin>&& impl): plugin(std::forward<std::shared_ptr<pressio_search_plugin>>(impl)) {}
  /** default constructor */
  pressio_search()=default;
  /** move constructor
   * \param[in] search the metrics to move from
   * */
  pressio_search(pressio_search&& search)=default;
  /** move assignment
   * \param[in] search the metrics to move from
   * */
  pressio_search& operator=(pressio_search&& search)=default;
  /**
   * \returns true if the plugin is non-empty
   */
  operator bool() const {
    return bool(plugin);
  }

  /** make pressio_search_plugin behave like a shared_ptr */
  pressio_search_plugin& operator*() const noexcept {
    return *plugin;
  }

  /** make pressio_search_plugin behave like a shared_ptr */
  pressio_search_plugin* operator->() const noexcept {
    return plugin.get();
  }

  /** the actual plugin */
  std::shared_ptr<pressio_search_plugin> plugin;
};


/** \returns a reference to the registry singleton */
pressio_registry<std::shared_ptr<pressio_search_plugin>>& search_plugins();