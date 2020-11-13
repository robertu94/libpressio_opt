#include "pressio_search_metrics.h"
#include <libpressio_ext/cpp/dtype.h>
#include <libpressio_ext/cpp/io.h>
#include <libpressio_ext/cpp/options.h>
#include <libpressio_ext/cpp/pressio.h>
#include <type_traits>
#include <vector>
#include <libpressio_ext/compat/memory.h>
#include <libpressio_ext/cpp/distributed_manager.h>

struct record_search : public pressio_search_metrics_plugin {
  void begin_search() override {
    results.clear();
    fields = 0;
    iterations = 0;
  }

  void end_iter(pressio_search_results::input_type const& input, pressio_search_results::output_type const& out) override {
    results.insert(results.end(), input.begin(), input.end());
    results.insert(results.end(), out.begin(), out.end());
    if(fields == 0) {
      fields = input.size() + out.size();
    } else if(fields != out.size() + input.size()) {
      set_error(1, "field sizes don't match");
    }
    iterations++;
  }

  void end_search(pressio_search_results::input_type const&, pressio_search_results::output_type const&) override {
    const int size=manager.comm_size(), rank=manager.comm_rank();
    if(rank == 0) {
      //first collect the results on the master process
      size_t tmp_iterations, tmp_fields;
      std::vector<value_type> tmp_results;
      for (int i = 1; i < size; ++i) {
        manager.recv(tmp_fields, i);
        manager.recv(tmp_iterations, i);
        manager.recv(tmp_results, i);
        fields = tmp_fields;
        iterations += tmp_iterations;
        results.insert(results.end(), tmp_results.begin(), tmp_results.end());
      }

      //write out results to a file
      auto data = pressio_data::nonowning(
          pressio_dtype_from_type<value_type>(),
          results.data(),
          {iterations, fields}
          );
      io->write(&data);

    } else {
      manager.send(fields, 0);
      manager.send(iterations, 0);
      manager.send(results, 0);
    }
  }

  pressio_options get_options() const override {
    pressio_options opts;
    set_meta(opts, "record_search:io_format", io_format, io);
    return opts;
  };

  int set_options(pressio_options const& opts) override {
    get_meta(opts, "record_search:io_format", io_plugins(), io_format, io);
    manager.set_options(opts);
    return 0;
  };

  virtual pressio_options get_metrics_results() override {
    return pressio_options();
  }

  const char* prefix() const override {
    return "record_search";
  }

  std::shared_ptr<pressio_search_metrics_plugin> clone() override {
    return compat::make_unique<record_search>(*this);
  }

  void set_name_impl(std::string const& new_name) override {
      io->set_name(new_name + "/" + io->prefix());
      manager.set_name(new_name);
  }


  private:
  using value_type = std::common_type<pressio_search_results::input_type::value_type,
                     pressio_search_results::output_type::value_type>::type;
  std::vector<value_type> results;
  size_t iterations = 0;
  size_t fields = 0;
  pressio_distributed_manager manager = pressio_distributed_manager(
      /*max_masters*/1,
      /*max_ranks_per_worker*/1
      );
  std::string io_format = "csv";
  pressio_io io = io_plugins().build("csv");
};
static pressio_register X(search_metrics_plugins(), "record_search", [](){ return compat::make_unique<record_search>();});
