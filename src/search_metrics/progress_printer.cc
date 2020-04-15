#include <iostream>
#include <iterator>
#include <string>
#include <sstream>
#include "pressio_search_metrics.h"
#include <mpi.h>

struct progress_printer : public pressio_search_metrics_plugin {

  progress_printer() {
    int is_init;
    MPI_Initialized(&is_init);
    if(is_init){
      int rank, size;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      MPI_Comm_size(MPI_COMM_WORLD, &size);
      std::stringstream r;
      r <<  rank << ',' << size << ',';
      rank_str = r.str();
    }
  }

  void begin_search() override {
    iteration = 0;
  }

  void end_iter(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
    format(format(std::cout << rank_str << iteration << ',',  input), out) << out.front() << std::endl;
    iteration++;
  }

  void end_search(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
    format(format(std::cout << "final iter=" << iteration << ": inputs=",  input) << " output=", out) << std::endl;
  }

  virtual pressio_options get_metrics_results() override {
    return pressio_options();
  }

  std::shared_ptr<pressio_search_metrics_plugin> clone() override {
    return compat::make_unique<progress_printer>(*this);
  }

  private:
  template <class CharT, class Traits>
  std::basic_ostream<CharT, Traits>&
  format(std::basic_ostream<CharT, Traits>& out, pressio_search_results::input_type const& input) {
    using element_type = pressio_search_results::input_type::value_type;
    std::copy(std::begin(input), std::end(input), std::ostream_iterator<element_type>(out, ","));
    return out;
  }

  size_t iteration = 0;
  std::string rank_str;
};

static pressio_register X(search_metrics_plugins(), "progress_printer", [](){ return compat::make_unique<progress_printer>();});
