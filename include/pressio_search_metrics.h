#ifndef PRESSIO_SEARCH_METRICS_H
#define PRESSIO_SEARCH_METRICS_H
#include<libpressio_ext/cpp/pressio.h>
#include<libpressio_ext/cpp/options.h>

#include "pressio_search_results.h"

/**
 * \file
 * \brief provides the definitions used for search_metrics plugins
 */

/**
 * base class for search metrics plugins
 */
struct pressio_search_metrics_plugin {
  virtual ~pressio_search_metrics_plugin()=default;

  //options are optional for metrics

  /** validate the options passed the search metrics module 
   * \param[in] options the configuration to check
   * \returns 0 if there was no error, non zero if there was a error
   * */
  virtual int check_metrics_options(pressio_options const& options) { return 0; }
  /** apply the options passed the search metrics module 
   * \param[in] options the configuration to apply
   * \returns 0 if there was no error, non zero if there was a error
   * */
  virtual int set_metrics_options(pressio_options const& options) { return 0; }
  /**
   * get the runtime settings of the search metrics module 
   *
   * \returns the options for this search metrics module
   */
  virtual pressio_options get_metrics_options() const { return pressio_options(); }

  //hooks for methods
  /**
   * called at the beginning of each iteration of the search
   *
   * \param[in] inputs the input to this iteration
   */
  virtual void begin_iter(pressio_search_results::input_type const& inputs){}
  /**
   * called at the end of each iteration of the search
   *
   * \param[in] inputs the input to this iteration
   * \param[in] out the input to this iteration
   */
  virtual void end_iter(pressio_search_results::input_type const& inputs, pressio_search_results::output_type out, pressio_search_results::objective_type objective){}
  /**
   * called at the beginning the entire search
   */
  virtual void begin_search(){}
  /**
   * called at the end of the entire search
   *
   * \param[in] inputs input that corresponds to the best configuration
   * \param[in] out output that corresponds to the best configuration
   */
  virtual void end_search(pressio_search_results::input_type const& inputs, pressio_search_results::objective_type out){}

  //get metrics results
  /**
   * \returns the results of the metrics if there are any
   */
  virtual pressio_options get_metrics_results()=0;

  /**
   * \returns a clone of the current search metrics object
   */
  virtual std::shared_ptr<pressio_search_metrics_plugin> clone()=0;

};


/**
 * wrapper interface for C usage if required
 */
struct pressio_search_metrics {
  /** constructor 
   * \param[in] impl the plugin to construct
   * */
  pressio_search_metrics(std::shared_ptr<pressio_search_metrics_plugin>&& impl): plugin(std::forward<std::shared_ptr<pressio_search_metrics_plugin>>(impl)) {}
  /** default constructor */
  pressio_search_metrics()=default;
  /** move constructor
   * \param[in] search_metrics the metrics to move from
   * */
  pressio_search_metrics(pressio_search_metrics&& search_metrics)=default;
  /** move assignment
   * \param[in] search_metrics the metrics to move from
   * */
  pressio_search_metrics& operator=(pressio_search_metrics&& search_metrics)=default;
  /**
   * \returns true if the plugin is non-empty
   */
  operator bool() const {
    return bool(plugin);
  }

  /** make pressio_search_metrics_plugin behave like a shared_ptr */
  pressio_search_metrics_plugin& operator*() const noexcept {
    return *plugin;
  }

  /** make pressio_search_metrics_plugin behave like a shared_ptr */
  pressio_search_metrics_plugin* operator->() const noexcept {
    return plugin.get();
  }

  /** the actual plugin */
  std::shared_ptr<pressio_search_metrics_plugin> plugin;
};


/** \returns a reference to the registry singleton */
pressio_registry<std::shared_ptr<pressio_search_metrics_plugin>>& search_metrics_plugins();


#endif /* end of include guard: PRESSIO_SEARCH_METRICS_H_D0TFAQ8Y */
