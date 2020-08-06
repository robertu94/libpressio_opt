#include <algorithm>
#include <sstream>
#include <iterator>
#include <mutex>
#include <condition_variable>
#include <mpi.h>
#include "pressio_compressor.h"
#include "libpressio_ext/cpp/pressio.h"
#include "libpressio_ext/cpp/data.h"
#include "libpressio_ext/cpp/compressor.h"
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/metrics.h"

#include "pressio_search.h"
#include "pressio_search_metrics.h"
#include "pressio_search_defines.h"
#include <libpressio_ext/compat/memory.h>

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
      set(options, "opt:inputs", input_settings);
      set(options, "opt:output", output_settings);
      set(options, "opt:do_decompress", do_decompress);
      set_type(options, "opt:objective_mode_name", pressio_option_charptr_type);
      set_meta(options, "opt:compressor", compressor_method, compressor);
      set_meta(options, "opt:search_metrics", search_metrics_method, search_metrics);
      set_meta(options, "opt:search", search_method, search, options);
      return options;
    }

    struct pressio_options get_configuration_impl() const override {
      struct pressio_options options;
      set(options,"pressio:thread_safe", (int)pressio_thread_safety_single);
      set(options,"opt:search_methods", get_registry_names(search_plugins()));
      set(options,"opt:search_metrics", get_registry_names(search_metrics_plugins()));
      auto compressor_configuration = compressor->get_configuration();
      for (auto const& option : compressor_configuration) {
        options.set(option.first, option.second);
      }
      
      return options;
    }

    int set_options_impl(struct pressio_options const& options) override {
      pressio_options search_options = options;
      search_options.set("opt:thread_safe", is_thread_safe());

      get_meta(search_options, "opt:compressor", compressor_plugins(), compressor_method, compressor);
      get_meta(search_options, "opt:search", search_plugins(), search_method, search);
      get_meta(search_options, "opt:search_metrics", search_metrics_plugins(), search_metrics_method, search_metrics);
      get(search_options, "opt:inputs", &input_settings);
      get(search_options, "opt:output", &output_settings);
      get(search_options, "opt:do_decompress", &do_decompress);


      std::string mode_name;
      if(get(search_options, "opt:objective_mode_name", &mode_name) == pressio_options_key_set) {
        unsigned int mode = 0;
        if(mode_name == "max") mode = pressio_search_mode_max;
        else if(mode_name == "min") mode = pressio_search_mode_min;
        else if(mode_name == "target") mode = pressio_search_mode_target;
        else if(mode_name == "none") mode = pressio_search_mode_none;

        set(search_options, "opt:objective_mode", mode);
        set_type(search_options, "opt:objective_mode_str", pressio_option_charptr_type);
      }

      
      return 0;
    }

    int compress_impl(const pressio_data* input_data,
                      struct pressio_data* output) override
    {
      if(output_settings.empty()) return output_required();

      bool run_metrics = true;
      auto metrics = get_metrics();
      compressor->set_metrics(metrics);

      auto compress_thread_fn = [&run_metrics, &input_data,&output,this](pressio_search_results::input_type const& input_v) {
        if(run_metrics) search_metrics->begin_iter(input_v);

        auto thread_compressor = compressor->clone();
        auto thread_output = pressio_data::clone(*output);

        //configure the compressor for this input
        auto settings = thread_compressor->get_options();
        for (int i = 0; i < input_v.size(); ++i) {
           if(settings.cast_set(input_settings[i], input_v[i], pressio_conversion_explicit) != pressio_options_key_set) {
             throw pressio_search_exception(std::string("failed to configure setting: ") + input_settings[i]);
           }
        }
        if(thread_compressor->set_options(settings)) {
             throw pressio_search_exception(std::string("failed to configure compressor: ") + thread_compressor->error_msg());
        }

        pressio_data decompressed;
        if(thread_compressor->compress(input_data, &thread_output)) {
             throw pressio_search_exception(std::string("failed to compress data: ") + thread_compressor->error_msg());
        }
        if(do_decompress) {
          decompressed = pressio_data::owning(input_data->dtype(), input_data->dimensions());
          if(thread_compressor->decompress(&thread_output, &decompressed)) {
             throw pressio_search_exception(std::string("failed to decompress data: ") + thread_compressor->error_msg());
          }
        }

        auto metrics_results = thread_compressor->get_metrics_results();

        std::vector<double> results;
        for (auto const& output_setting : output_settings) {
          double result;
          if(metrics_results.cast(output_setting, &result, pressio_conversion_explicit) != pressio_options_key_set) {
               throw pressio_search_exception(std::string("failed to retrieve metric: ") + output_setting);
          }
          results.push_back(result);
        }

        if(run_metrics) search_metrics->end_iter(input_v, results);
        return results;
      };

      auto compress_fn = [&run_metrics, &input_data,&output,this](pressio_search_results::input_type const& input_v) {
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

        if(run_metrics) search_metrics->end_iter(input_v, results);
        return results;
      };

      try {
        OptStopToken token;
        search_metrics->begin_search();
        last_results = search->search(compress_thread_fn, token);
        search_metrics->end_search(last_results->inputs, last_results->output);
        //set metrics results to the results metrics
        run_metrics = false;
        compress_fn(last_results->inputs);
        if(last_results->status) return set_error(last_results->status, last_results->msg);
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

    void set_name_impl(std::string const& new_name) override {
      compressor->set_name(new_name + "/" + compressor->prefix());
      search->set_name(new_name + "/" + search->prefix());
      search_metrics->set_name(new_name + "/" + search_metrics->prefix());
    }

    std::shared_ptr<libpressio_compressor_plugin> clone() override {
      auto tmp = compat::make_unique<pressio_opt_plugin>();
      tmp->library = library;

      tmp->compressor = compressor->clone();
      tmp->compressor_method = compressor_method;

      tmp->search = search->clone();
      tmp->search_method = search_method;

      tmp->search_metrics = search_metrics->clone();

      tmp->input_settings = input_settings;
      tmp->output_settings = output_settings;
      tmp->do_decompress = do_decompress;
      return tmp;
    }

    pressio_options get_metrics_results_impl() const override {
      auto search_metrics_results = search_metrics->get_metrics_results();
      if(last_results) {
        set(search_metrics_results, "opt:input", pressio_data(std::begin(last_results->inputs), std::end(last_results->inputs)));
        set(search_metrics_results, "opt:output", pressio_data(std::begin(last_results->output), std::end(last_results->output)));
        set(search_metrics_results, "opt:msg", last_results->msg);
        set(search_metrics_results, "opt:status", last_results->status);
      } else {
        set_type(search_metrics_results, "opt:input", pressio_option_data_type);
        set_type(search_metrics_results, "opt:output", pressio_option_data_type);
        set_type(search_metrics_results, "opt:msg", pressio_option_charptr_type);
        set_type(search_metrics_results, "opt:status", pressio_option_int32_type);
      }
      return search_metrics_results;
    }


  private:
    int is_thread_safe() const {
      int mpi_init=0;
      MPI_Initialized(&mpi_init);

      int compressor_thread_safety=0;
      compressor->get_configuration().get("pressio:thread_safe", &compressor_thread_safety);

      if(mpi_init) {
        int mpi_thread_provided;
        MPI_Query_thread(&mpi_thread_provided);
        return compressor_thread_safety == pressio_thread_safety_multiple && mpi_thread_provided == MPI_THREAD_MULTIPLE;
      } else {
        return compressor_thread_safety == pressio_thread_safety_multiple;
      }
    }
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
    compat::optional<pressio_search_results> last_results;

    std::string compressor_method="noop";
    std::string search_method="guess";
    std::string search_metrics_method="progress_printer";
    std::vector<std::string> input_settings{};
    std::vector<std::string> output_settings;
    int do_decompress = 1;
};

static pressio_register X(compressor_plugins(), "opt", [](){ return compat::make_unique<pressio_opt_plugin>(); });
