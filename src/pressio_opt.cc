#include <algorithm>
#include <sstream>
#include <iterator>
#include "pressio_compressor.h"
#include "libpressio_ext/cpp/pressio.h"
#include "libpressio_ext/cpp/data.h"
#include "libpressio_ext/cpp/compressor.h"
#include "libpressio_ext/cpp/options.h"

#include "pressio_search.h"
#include "pressio_search_metrics.h"

namespace {
  template <class Registry>
  std::vector<std::string> get_registry_names(Registry const& plugins) {
    std::vector<std::string> names;
    std::transform(std::begin(plugins), std::end(plugins),
                   std::back_inserter(names),
                   [](auto const& it) { return it.first; });
    return names;
}

}

class pressio_opt_plugin: public libpressio_compressor_plugin {
  public:
    pressio_opt_plugin() {
      compressor = library.get_compressor(compressor_method);
      search = search_plugins().build(search_method);
      metrics = search_metrics_plugins().build(search_metrics_method);
    }

    struct pressio_options get_options_impl() const override {
      struct pressio_options options;
      options.set("opt:compressor", compressor_method);
      options.set("opt:search", search_method);
      options.set("opt:inputs", inputs);
      options.set("opt:output", output);
      options.set("opt:do_decompress", do_decompress);
      options.set("opt:search_metrics", search_metrics_method);
      auto search_options = search->get_options(options);
      for (auto const& option : search_options) {
        options.set(option.first, option.second);
      }

      return options;
    }

    struct pressio_options get_configuration_impl() const override {
      struct pressio_options options;
      options.set("pressio:thread_safe", (int)pressio_thread_safety_single);
      options.set("opt:search_methods", get_registry_names(search_plugins()));
      options.set("opt:search_metrics", get_registry_names(search_metrics_plugins()));
      return options;
    }

    int set_options_impl(struct pressio_options const& options) override {
      options.get("opt:compressor", &compressor_method);
      options.get("opt:search", &search_method);
      options.get("opt:inputs", &inputs);
      options.get("opt:output", &output);
      options.get("opt:do_decompress", &do_decompress);
      return 0;
    }

    int compress_impl(const pressio_data *input, struct pressio_data* output) override {
      if(not output) return output_required();
      return 0;
    }

    int decompress_impl(const pressio_data *input, struct pressio_data* output) override {
      return 0;
    }

    int major_version() const override {
      return 0;
    }
    int minor_version() const override {
      return 0;
    }
    int patch_version() const override {
      return 0;
    }

    const char* version() const override {
      return 0;
    }

    const char* prefix() const override {
      return "opt";
    }



  private:
    int output_required() {
      return set_error(1, "opt:output is required to be set, but is not");
    }

    pressio library{};
    pressio_compressor compressor;
    pressio_search search;
    pressio_search_metrics metrics;

    std::string compressor_method="noop";
    std::string search_method="guess";
    std::string search_metrics_method="progress_printer";
    std::vector<std::string> inputs{};
    std::string output;
    int do_decompress = 1;
};

static pressio_register X(compressor_plugins(), "opt", [](){ return compat::make_unique<pressio_opt_plugin>(); });
