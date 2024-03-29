#include <stdexcept>
#include <functional>
#include <libpressio_ext/cpp/pressio.h>
#include <libpressio_ext/cpp/versionable.h>
#include <libpressio_ext/cpp/configurable.h>
#include <libpressio_ext/cpp/options.h>
#include <libdistributed_task_manager.h>
#include "pressio_search_results.h"

/**
 * \file
 * \brief interface for search modules
 */


class pressio_search_exception : public std::runtime_error {
  using runtime_error::runtime_error;
};

/**
 * base class for search plugins
 */
struct pressio_search_plugin : public pressio_versionable, public pressio_configurable  {
  public:
    std::string type() const final {
        return "search";
    }
    virtual pressio_options get_configuration_impl() const {
        return {};
    }
    pressio_options get_configuration() const final {
        auto opts = get_configuration_impl();
        set(opts, "pressio:children", children());
        set(opts, "pressio:type", type());
        set(opts, "pressio:prefix", prefix());
        return opts;
    }

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
    virtual pressio_search_results search(compat::span<const pressio_data *const> const &input_datas,
                                          std::function<pressio_search_results::output_type(
                                                  pressio_search_results::input_type const &)> compress_fn,
                                          distributed::queue::StopToken &stop_token) =0;

    /**
     * \returns a clone of the current search object
     */
    virtual std::shared_ptr<pressio_search_plugin> clone()=0;

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
  pressio_search(pressio_search const& rhs): plugin(rhs->clone()) {};

  pressio_search& operator=(pressio_search const& rhs) {
    if(this == &rhs) return *this;
    plugin = rhs->clone();
    return *this;
  };

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
