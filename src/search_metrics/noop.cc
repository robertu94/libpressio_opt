#include <iostream>
#include <iterator>
#include <string>
#include "pressio_search_metrics.h"
#include "pressio_search_results.h"
#include <std_compat/memory.h>


struct noop_search_metric : public pressio_search_metrics_plugin {

  noop_search_metric() {
  }

  void begin_search() override {
  }

  void end_iter(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
  }

  void end_search(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
  }

  virtual pressio_options get_metrics_results() override {
    return pressio_options();
  }

  const char* prefix() const override {
    return "noop";
  }

  std::shared_ptr<pressio_search_metrics_plugin> clone() override {
    return compat::make_unique<noop_search_metric>(*this);
  }
};

static pressio_register X(search_metrics_plugins(), "noop", [](){ return compat::make_unique<noop_search_metric>();});
