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
#include "libpressio_opt_version.h"
#include <std_compat/memory.h>

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


    struct pressio_options get_documentation_impl() const override {
      struct pressio_options options;
      set_meta_docs(options, "opt:compressor", "the compressor to optimize over", compressor);
      set_meta_docs(options, "opt:search_metrics", "search metrics to collect", search_metrics);
      set_meta_docs(options, "opt:search", "search method to use", search);
      set(options, "pressio:description", "Uses optimization to automatically configure compressors");
      set(options, "opt:objective_mode_name", "the name for the optimization method");
      set(options, "opt:inputs", "list of input settings");
      set(options, "opt:output", "list of output settings");
      set(options, "opt:do_decompress", "preform decompression while tuning");
      set(options, "opt:prediction", "guess of the optimal configuration");
      return options;
    }
    struct pressio_options get_options_impl() const override {
      struct pressio_options options;
      set_meta(options, "opt:compressor", compressor_method, compressor);
      set_meta(options, "opt:search_metrics", search_metrics_method, search_metrics);
      set_meta(options, "opt:search", search_method, search);
      set_type(options, "opt:objective_mode_name", pressio_option_charptr_type);
      set(options, "opt:inputs", input_settings);
      set(options, "opt:output", output_settings);
      set(options, "opt:do_decompress", do_decompress);
      return options;
    }

    struct pressio_options get_configuration_impl() const override {
      struct pressio_options options;
      options.copy_from(compressor->get_configuration());
      options.copy_from(search->get_configuration());
      options.copy_from(search_metrics->get_configuration());
      set(options,"pressio:thread_safe", (int)pressio_thread_safety_single);
      set(options,"opt:search", get_registry_names(search_plugins()));
      set(options,"opt:search_metrics", get_registry_names(search_metrics_plugins()));
      return options;
    }

    int set_options_impl(struct pressio_options const& options) override {
      pressio_options search_options = options;
      std::string mode_name;
      if(get(search_options, "opt:objective_mode_name", &mode_name) == pressio_options_key_set) {
        unsigned int mode = 0;
        if(mode_name == "max") mode = pressio_search_mode_max;
        else if(mode_name == "min") mode = pressio_search_mode_min;
        else if(mode_name == "target") mode = pressio_search_mode_target;
        else if(mode_name == "none") mode = pressio_search_mode_none;

        search_options.set(search->get_name(), "opt:objective_mode", mode);
        search_options.set_type(search->get_name(), "opt:objective_mode_name", pressio_option_charptr_type);
      }

      get_meta(search_options, "opt:compressor", compressor_plugins(), compressor_method, compressor);
      //the search needs to know if the compressor is thread_safe, and can only
      //check if that is true, after the compressor has been configured
      search_options.set("opt:thread_safe", is_thread_safe());

      get_meta(search_options, "opt:search", search_plugins(), search_method, search);
      get_meta(search_options, "opt:search_metrics", search_metrics_plugins(), search_metrics_method, search_metrics);
      get(search_options, "opt:inputs", &input_settings);
      get(search_options, "opt:output", &output_settings);
      get(search_options, "opt:do_decompress", &do_decompress);


      
      return 0;
    }

    int compress_many_impl(compat::span<const pressio_data* const> const& input_datas,
                      compat::span<struct pressio_data*>& outputs) override
    {
      if(output_settings.empty()) return output_required();
      if(input_settings.empty()) return input_required();

      bool run_search_metrics = true;

      auto common_compress_thread_fn = [&run_search_metrics, &input_datas,
                                 this](pressio_search_results::input_type const&
                                         input_v, pressio_compressor& thread_compressor, compat::span<pressio_data*>& thread_outputs) {
        if (run_search_metrics)
          search_metrics->begin_iter(input_v);

        if(input_v.size() != input_settings.size()) {
            throw pressio_search_exception(
              std::string("mismatched number of inputs inputs=") + std::to_string(input_v.size()) + " settings=" + std::to_string(input_settings.size()));
        }

        //configure the compressor for this input
        auto settings = thread_compressor->get_options();
        for (size_t i = 0; i < input_v.size(); ++i) {
          if (settings.cast_set(input_settings[i], input_v[i],
                                pressio_conversion_explicit) !=
              pressio_options_key_set) {
            throw pressio_search_exception(
              std::string("failed to configure setting: ") + input_settings[i]);
          }
        }
        if(thread_compressor->set_options(settings)) {
          throw pressio_search_exception(
            std::string("failed to configure compressor: ") +
            thread_compressor->error_msg());
        }

        pressio_data decompressed;
        if(thread_compressor->compress_many(
              input_datas.data(),
              input_datas.data()+input_datas.size(),
              thread_outputs.data(),
              thread_outputs.data()+thread_outputs.size())) {
          throw pressio_search_exception(
            std::string("failed to compress data: ") +
            thread_compressor->error_msg());
        }

        if(do_decompress) {
          std::vector<pressio_data> decompressed;
          std::vector<pressio_data*> decompressed_ptrs;
          std::transform(
              std::begin(input_datas),
              std::end(input_datas),
              std::back_inserter(decompressed),
              [](const pressio_data * input_data) {
                return pressio_data::owning(input_data->dtype(), input_data->dimensions());
              });
          std::transform(
              std::begin(decompressed),
              std::end(decompressed),
              std::back_inserter(decompressed_ptrs),
              [](pressio_data& decompressed) {
                return &decompressed;
              });
          if(thread_compressor->decompress_many(
                thread_outputs.data(),
                thread_outputs.data()+thread_outputs.size(),
                decompressed_ptrs.data(),
                decompressed_ptrs.data()+decompressed_ptrs.size())) {
            throw pressio_search_exception(
              std::string("failed to decompress data: ") +
              thread_compressor->error_msg());
          }
        }

        auto metrics_results = thread_compressor->get_metrics_results();

        std::vector<double> results;
        for (auto const& output_setting : output_settings) {
          double result;
          if(metrics_results.find(output_setting) == metrics_results.end()) {
            throw pressio_search_exception(
              std::string("metric does not exist: ") + output_setting);
          }
          if(metrics_results.cast(output_setting, &result, pressio_conversion_explicit) != pressio_options_key_set) {
            throw pressio_search_exception(
              std::string("metric is not convertible to double: ") +
              output_setting);
          }
          results.push_back(result);
        }

        if (run_search_metrics)
          search_metrics->end_iter(input_v, results);
        return results;
      };

      auto compress_thread_fn = [&outputs, &common_compress_thread_fn,
                                 this](pressio_search_results::input_type const&
                                         input_v) {
        pressio_compressor thread_compressor = compressor->clone();
        std::vector<pressio_data> thread_outputs;
        std::vector<pressio_data*> thread_outputs_ptrs;
        std::transform(std::begin(outputs), std::end(outputs), std::back_inserter(thread_outputs),
            [](pressio_data* data) {
              return pressio_data::clone(*data);
            });
        std::transform(std::begin(thread_outputs), std::end(thread_outputs), std::back_inserter(thread_outputs_ptrs),
            [](pressio_data& data) {
              return &data;
            });
        compat::span<pressio_data*> thread_outputs_ptrs_span (thread_outputs_ptrs.data(), thread_outputs_ptrs.data()+thread_outputs_ptrs.size());
        return common_compress_thread_fn(input_v, thread_compressor, thread_outputs_ptrs_span);
      };

      auto compress_fn = [&outputs, &common_compress_thread_fn, this](
                           pressio_search_results::input_type const& input_v) {
        return common_compress_thread_fn(input_v, compressor, outputs);
      };

      try {
        OptStopToken token;
        search_metrics->begin_search();
        last_results = search->search(input_datas, compress_thread_fn, token);
        search_metrics->end_search(last_results->inputs, last_results->output);
        //set metrics results to the results metrics
        run_search_metrics = false;
        if(last_results->status) {
          return set_error(last_results->status, last_results->msg);
        } else {
          compress_fn(last_results->inputs);
          return 0;
        }
      } catch(pressio_search_exception const& e) {
        return set_error(2, e.what());
      }

    }
    int compress_impl(const pressio_data *input, struct pressio_data* output) override {
      const compat::span<pressio_data const*const> inputs(&input, 1);
      compat::span<pressio_data*> outputs(&output, 1);
      return compress_many_impl(inputs, outputs);
    }

    int decompress_impl(const pressio_data *input, struct pressio_data* output) override {
      const compat::span<pressio_data const*const> inputs(&input, 1);
      compat::span<pressio_data*> outputs(&output, 1);
      return decompress_many_impl(inputs, outputs);
    }
    int decompress_many_impl(const compat::span<pressio_data const*const>& inputs, compat::span<struct pressio_data*>& outputs) override {
      return compressor->decompress_many(
          inputs.data(),
          inputs.data()+inputs.size(),
          outputs.data(),
          outputs.data()+outputs.size()
          );
    }

    int major_version() const override {
      return LIBPRESSIO_OPT_MAJOR_VERSION;
    }
    int minor_version() const override {
      return LIBPRESSIO_OPT_MINOR_VERSION;
    }
    int patch_version() const override {
      return LIBPRESSIO_OPT_PATCH_VERSION;
    }

    const char* version() const override {
      static const std::string opt_version_str = []{
        std::stringstream ss;
        ss << LIBPRESSIO_OPT_MAJOR_VERSION << '.'
           << LIBPRESSIO_OPT_MINOR_VERSION << '.'
           << LIBPRESSIO_OPT_PATCH_VERSION;
        return ss.str();
      }();
      return opt_version_str.c_str();
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

      int compressor_thread_safety = get_threadsafe(*compressor);

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
    int input_required() {
      return set_error(3, "opt:input is required to be set, but is not");
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
