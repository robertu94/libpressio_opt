#include <iostream>
#include <iterator>
#include "pressio_search_metrics.h"

struct progress_printer : public pressio_search_metrics_plugin {

  void end_iter(pressio_search_results::input_type const& input, pressio_search_results::output_type out) override {
    format(format(std::cout << "iter=" << iteration << ": inputs=",  input) << " output= ", out) << std::endl;
    iteration++;
  }

  void end_search(pressio_search_results::input_type const& input, pressio_search_results::output_type out) override {
    format(format(std::cout << "final iter=" << iteration << ": inputs=",  input) << " output= ", out) << std::endl;
  }

  virtual pressio_options get_metrics_results() override {
    return pressio_options();
  }


  private:
  template <class CharT, class Traits>
  std::basic_ostream<CharT, Traits>&
  format(std::basic_ostream<CharT, Traits>& out, pressio_search_results::input_type const& input) {
    using element_type = pressio_search_results::input_type::value_type;
    std::copy(std::begin(input), std::end(input), std::ostream_iterator<element_type>(out, ", "));
    return out;
  }
  template <class CharT, class Traits>
  std::basic_ostream<CharT, Traits>&
  format(std::basic_ostream<CharT, Traits>& out, pressio_search_results::output_type const& output) {
    return out << output;
  }

  size_t iteration = 0;
};

static pressio_register X(search_metrics_plugins(), "progress_printer", [](){ return compat::make_unique<progress_printer>();});
