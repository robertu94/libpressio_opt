#include "pressio_search_metrics.h"
#include <cstdio>
#include <cstdlib>
#include <libpressio_ext/cpp/options.h>
#include <libpressio_ext/cpp/pressio.h>
#include <memory>
#include <vector>
#include <std_compat/memory.h>

struct composite_search_metrics : public pressio_search_metrics_plugin {
  void begin_search() override {
    for (auto& plugin : plugins) {
      plugin->begin_search();
    }
  }


  void begin_iter(pressio_search_results::input_type const& input) override {
    for (auto& plugin : plugins) {
      plugin->begin_iter(input);
    }
  }

  void end_iter(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
    for (auto& plugin : plugins) {
      plugin->end_iter(input, out);
    }
  }

  void end_search(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
    for (auto& plugin : plugins) {
      plugin->end_search(input, out);
    }
  }

  pressio_options get_options() const override {
    pressio_options opts;
    set(opts, "composite_search:search_metrics", search_metrics);
    for (auto& plugin : plugins) {
      auto plugin_options = plugin->get_options();
      for(auto& plugin_option : plugin_options) {
        opts.set(plugin_option.first, plugin_option.second);
      }
    }
    return opts;
  };

  int set_options(pressio_options const& opts) override {
    //check for update to search_metrics_plugins
    std::vector<std::string> tmp_search_metrics;
    if(get(opts, "composite_search:search_metrics", &tmp_search_metrics) == pressio_options_key_set) {
      if(tmp_search_metrics != search_metrics) {
        std::vector<pressio_search_metrics> new_plugins;
        new_plugins.reserve(tmp_search_metrics.size());
        for (auto const& new_plugin_id : tmp_search_metrics) {
          new_plugins.emplace_back(search_metrics_plugins().build(new_plugin_id));
          if(not new_plugins.back()) {
            return set_error(1, std::string("failed_to load search_metrics_plugin: ") + new_plugin_id);
          } 
        }
        search_metrics = std::move(tmp_search_metrics);
        plugins = std::move(new_plugins);
        if(not get_name().empty()) {
          set_name(get_name());
        }
      }
    }

    //check for updates to search_metrics_plugin names
    std::vector<std::string> tmp_search_metrics_names;
    if(get(opts, "composite_search:names", &tmp_search_metrics_names) == pressio_options_key_set) {
      if(tmp_search_metrics_names != search_metrics_names) {
        //if the names array is not empty not equal to plugin size return an error
        if(tmp_search_metrics_names.size() > 0 && tmp_search_metrics_names.size() != plugins.size()) {
          set_error(2, "invalid number of names");
        }

        for (size_t i = 0; i < plugins.size(); ++i) {
          if(not get_name().empty()) {
            plugins[i]->set_name(get_name() + "/" + tmp_search_metrics_names[i]);
          } else {
            plugins[i]->set_name(tmp_search_metrics_names[i]);
          }
        }
        search_metrics_names = std::move(tmp_search_metrics_names);
      }
    }

    //call set_options on children

    for (auto& plugin : plugins) {
      int ret = plugin->set_options(opts);
      if(ret) {
        return ret;
      }
    }

    return 0;
  };

  virtual pressio_options get_metrics_results() override {
    pressio_options options;
    for (auto & i : plugins) {
      options.copy_from(i->get_metrics_results());
    }
    return options;
  }

  const char* prefix() const override {
    return "composite_search";
  }

  std::shared_ptr<pressio_search_metrics_plugin> clone() override {
    return compat::make_unique<composite_search_metrics>(*this);
  }


  private:
  std::vector<std::string> search_metrics_names;
  std::vector<std::string> search_metrics;
  std::vector<pressio_search_metrics> plugins;
};
static pressio_register X(search_metrics_plugins(), "composite_search", [](){ return compat::make_unique<composite_search_metrics>();});
