#include <algorithm>
#include <sstream>
#include <iterator>
#include "pressio_compressor.h"
#include "libpressio_ext/cpp/pressio.h"
#include "libpressio_ext/cpp/data.h"
#include "libpressio_ext/cpp/compressor.h"
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/metrics.h"

#include "pressio_search.h"
#include "pressio_search_metrics.h"
#include "pressio_search_exception.h"
#include "pressio_search_defines.h"

namespace {
  template <class Registry>
  std::vector<std::string> get_registry_names(Registry const& plugins) {
    std::vector<std::string> names;
    std::transform(std::begin(plugins), std::end(plugins),
                   std::back_inserter(names),
                   [](auto const& it) { return it.first; });
    return names;
}

class OptStopToken: public distributed::queue::StopToken {
  bool stop_requested() {
    return should_stop;
  }

  void request_stop() {
    should_stop = true;
  }

  bool should_stop = false;
};

}

class pressio_opt_plugin: public libpressio_compressor_plugin {
  public:
    pressio_opt_plugin() {
      compressor = library.get_compressor(compressor_method);
      search = search_plugins().build(search_method);
      search_metrics = search_metrics_plugins().build(search_metrics_method);
    }

    struct pressio_options get_options_impl() const override {
      struct pressio_options options;
      options.set("opt:compressor", compressor_method);
      options.set("opt:search", search_method);
      options.set("opt:inputs", input_settings);
      options.set("opt:output", output_settings);
      options.set("opt:do_decompress", do_decompress);
      options.set("opt:search_metrics", search_metrics_method);
      options.set("opt:objective_fn", (void*)multiobjective);
      options.set("opt:objective_data", (void*)multiobjective_data);
      auto search_options = search->get_options(options);
      for (auto const& option : search_options) {
        options.set(option.first, option.second);
      }
      auto compressor_options = compressor->get_options();
      for (auto const& option : compressor_options) {
        options.set(option.first, option.second);
      }

      return options;
    }

    struct pressio_options get_configuration_impl() const override {
      struct pressio_options options;
      options.set("pressio:thread_safe", (int)pressio_thread_safety_single);
      options.set("opt:search_methods", get_registry_names(search_plugins()));
      options.set("opt:search_metrics", get_registry_names(search_metrics_plugins()));
      auto compressor_configuration = compressor->get_configuration();
      for (auto const& option : compressor_configuration) {
        options.set(option.first, option.second);
      }
      
      return options;
    }

    int set_options_impl(struct pressio_options const& options) override {
      if(options.get("opt:compressor", &compressor_method) == pressio_options_key_set) {
        compressor = library.get_compressor(compressor_method);
      }
      if(options.get("opt:search", &search_method) == pressio_options_key_set) {
        search = search_plugins().build(search_method);
        if(!search) {
          return invalid_search_plugin(search_method);
        }
      }
      options.get("opt:inputs", &input_settings);
      options.get("opt:output", &output_settings);
      options.get("opt:do_decompress", &do_decompress);
      options.get("opt:objective_fn", (void**)&multiobjective);
      options.get("opt:objective_data", (void**)&multiobjective_data);

      compressor->set_options(options);
      search->set_options(options);
      return 0;
    }

    int compress_impl(const pressio_data* input_data,
                      struct pressio_data* output) override
    {
      if(output_settings.empty()) return output_required();

      bool run_metrics = true;
      auto metrics = get_metrics();
      auto compress_fn = [&run_metrics, &input_data,&metrics,&output,this](pressio_search_results::input_type const& input_v) {
        if(run_metrics) search_metrics->begin_iter(input_v);
        //configure the compressor for this input
        auto settings = compressor->get_options();
        for (int i = 0; i < input_v.size(); ++i) {
           if(settings.cast_set(input_settings[i], input_v[i], pressio_conversion_explicit) != pressio_options_key_set) {
             throw pressio_search_exception(std::string("failed to configure setting: ") + input_settings[i]);
           }
        }
        if(compressor->set_options(settings)) {
             throw pressio_search_exception(std::string("failed to configure compressor: ") + compressor->error_msg());
        }
        compressor->set_metrics(metrics);

        pressio_data decompressed;
        if(compressor->compress(input_data, output)) {
             throw pressio_search_exception(std::string("failed to compress data: ") + compressor->error_msg());
        }
        if(do_decompress) {
          decompressed = pressio_data::owning(input_data->dtype(), input_data->dimensions());
          if(compressor->decompress(output, &decompressed)) {
             throw pressio_search_exception(std::string("failed to decompress data: ") + compressor->error_msg());
          }
        }

        auto metrics_results = compressor->get_metrics_results();

        std::vector<double> results;
        for (auto const& output_setting : output_settings) {
          double result;
          if(metrics_results.cast(output_setting, &result, pressio_conversion_explicit) != pressio_options_key_set) {
               throw pressio_search_exception(std::string("failed to retrieve metric: ") + output_setting);
          }
          results.push_back(result);
        }
        double result = multiobjective(results.data(), results.size(), multiobjective_data);
        if(run_metrics) search_metrics->end_iter(input_v, results, result);
        return result;
      };

      try {
        OptStopToken token;
        search_metrics->begin_search();
        auto results = search->search(compress_fn, token);
        search_metrics->end_search(results.inputs, results.objective);
        //set metrics results to the results metrics
        run_metrics = false;
        compress_fn(results.inputs);
        if(results.status) return set_error(results.status, results.msg);
        return 0;
      } catch(pressio_search_exception const& e) {
        return set_error(2, e.what());
      }

    }

    int decompress_impl(const pressio_data *input, struct pressio_data* output) override {
      return compressor->decompress(input, output);
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
      return "0.0.0";
    }

    const char* prefix() const override {
      return "opt";
    }

    std::shared_ptr<libpressio_compressor_plugin> clone() override {
      auto tmp = compat::make_unique<pressio_opt_plugin>();
      tmp->library = library;
      tmp->compressor = compressor->clone();
      tmp->search = search->clone();
      tmp->search_metrics = search_metrics->clone();
      tmp->compressor_method = compressor_method;
      tmp->input_settings = input_settings;
      tmp->output_settings = output_settings;
      tmp->do_decompress = do_decompress;
      tmp->multiobjective = multiobjective;
      tmp->multiobjective_data = multiobjective_data;
      return tmp;
    }



  private:
    int output_required() {
      return set_error(1, "opt:output is required to be set, but is not");
    }
    int invalid_search_plugin(std::string const& name) {
      return set_error(2, name + " unknown search plugin");
    }

    pressio library{};
    pressio_compressor compressor{};
    pressio_search search{};
    pressio_search_metrics search_metrics{};

    std::string compressor_method="noop";
    std::string search_method="guess";
    std::string search_metrics_method="progress_printer";
    std::vector<std::string> input_settings{};
    std::vector<std::string> output_settings;
    pressio_opt_multiobjective_fn multiobjective = pressio_opt_multiobjective_first;
    void* multiobjective_data;
    int do_decompress = 1;
};

static pressio_register X(compressor_plugins(), "opt", [](){ return compat::make_unique<pressio_opt_plugin>(); });
