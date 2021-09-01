#include "pressio_search.h"
#include "pressio_search_results.h"
#include <std_compat/numeric.h>
#include <std_compat/memory.h>
#include <algorithm>

struct guess_midpoint_search: public pressio_search_plugin {
  public:
    pressio_search_results search(compat::span<const pressio_data *const> const &input_datas,
                                  std::function<pressio_search_results::output_type(
                                          pressio_search_results::input_type const &)> compress_fn,
                                  distributed::queue::StopToken &) override {
      pressio_search_results::input_type midpoint(lower_bound.size());
      using value_type = pressio_search_results::input_type::value_type;
      std::transform(
          std::begin(lower_bound),
          std::end(lower_bound),
          std::begin(upper_bound),
          std::begin(midpoint),
          [](value_type lower, value_type upper) {
            return compat::midpoint(lower, upper);
          }
          );

      pressio_search_results results{};
      results.inputs = midpoint;
      results.output = compress_fn(midpoint);
      return results;
    }

    //configuration
    pressio_options get_options() const override {
      pressio_options opts;
      set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(lower_bound)));
      set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));
      return opts;
    }

    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(get(options, "opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      if(get(options, "opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "guess_midpoint";
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
      return compat::make_unique<guess_midpoint_search>(*this);
    }

private:
    pressio_search_results::input_type lower_bound;
    pressio_search_results::input_type upper_bound;
};


static pressio_register guess_midpoint_register(search_plugins(), "guess_midpoint", [](){ return compat::make_unique<guess_midpoint_search>();});
