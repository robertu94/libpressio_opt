/**
 * A LibPressio-Opt search module for SDRSearch
 *
 *  A few important methods:
 *
 * sdr_search::search - train and/or estimate the parameters of a compressor that will result meet the user's objectives
 * sdr_search::get_options - get the options previously set on this module
 * sdr_search::set_options - set the options on this module
 *
 */
// {{{ includes
#include <algorithm>
#include <iostream>
#include <chrono>
#include "pressio_search.h"
#include <std_compat/std_compat.h>
// }}}

struct sdr_search: public pressio_search_plugin {
  public:
    /**
     *
     * \param[in] input_datas think of this as an array of immutable data arrays.  This it the dataset provided for training
     * \param[in] compress_fn computes the users' metric on the set of buffers input_datas
     * \param[in] token provides a means of 1) alerting other modules that this search has finished, or 2) detecting that another module has stopped
     * \returns the results of the search
     */
    pressio_search_results search(compat::span<const pressio_data *const> const &input_datas,
                                  std::function<pressio_search_results::output_type(
                                          pressio_search_results::input_type const &)> compress_fn,
                                  distributed::queue::StopToken &token) override {
      pressio_search_results results;

      //TODO implmement your analysis and search here
      std::cout << "did work!" << std::endl;

      results.inputs = pressio_search_results::input_type(inputs.size()); // the input that the user should use
      results.output = compress_fn(results.inputs); //what result do we get with these inputs?
      results.status = 0; // for success, <0 for warning, >0 for error
      results.msg = ""; //when status !=0, provide an explanation
      return results;
    }

    //configuration {{{
    pressio_options get_documentation() const override {
      pressio_options opts;
      set(opts, "opt:max_seconds", "the max time that the search is allocated takes in seconds; the search will stop as soon as possible after the deadline");
      set(opts, "opt:inputs", "the list of inputs");
      return opts;
    }
    pressio_options get_options() const override {
      pressio_options opts;
      
      set(opts, "opt:max_seconds", max_seconds);
      set(opts, "opt:inputs", inputs);
      return opts;
    }
    int set_options(pressio_options const& options) override {
      get(options, "opt:max_seconds", &max_seconds);
      get(options, "opt:inputs", &inputs);
      return 0;
    }
    // }}}
    //meta-data {{{
    /** get the prefix used by this compressor for options */
    virtual const char* prefix() const override {
      return "sdr";
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
    // }}}
    // required utility function {{{
    std::shared_ptr<pressio_search_plugin> clone() override {
      return compat::make_unique<sdr_search>(*this);
    }
    // }}}

private:
    //define local configuration options here, you can use the following types
    // int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, 
    // std::string (think char*, but memory mangaged), std::vector<std::string> (think char**, but memory manged)
    // pressio_data (array of input data), and void* (i.e. to hold a MPI_COMM or cudaStream_t)

    // a few other types are possible here in more advanced usage, so please ask if you need more.
    // be sure to configure these in get_options and set_options
    uint64_t max_seconds = 0; //0 means unlimited
    std::vector<std::string> inputs;
};

// autoloading boilerplate {{{
static pressio_register sdr_search_register(search_plugins(), "sdr", [](){ return compat::make_unique<sdr_search>();});
// }}}

// vim: foldmethod=marker foldmarker={{{,}}} :
