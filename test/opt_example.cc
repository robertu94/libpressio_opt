#include "libpressio_opt_ext/impl/pressio_data_utilities.h"
#include <sz/sz.h>
#include <iostream>
#include <libpressio.h>
#include <libpressio_ext/io/posix.h>
#include <libpressio_ext/cpp/libpressio.h>
#include <libpressio_opt_ext/impl/pressio_data_utilities.h>

float* make_data() {
  size_t idx = 0;
  auto data = static_cast<float*>(malloc(sizeof(float)*500*500*100));
  for (int i = 0; i < 500; ++i) {
    for (int j = 0; j < 500; ++j) {
      for (int k = 0; k < 100; ++k) {
        data[++idx] = i*i + 2*j - k;
      }
    }
  }
  return data;
}

int main(int argc, char *argv[])
{
  pressio library;
  std::string metrics_ids[] = {"size", "time"};
  pressio_metrics metrics = library.get_metrics(std::begin(metrics_ids), std::end(metrics_ids));
  auto compressor = library.get_compressor("opt");
  auto configuration = compressor->get_configuration();
  compressor->set_metrics(metrics);
  std::cout << "configuration:" << std::endl << configuration << std::endl;

  auto options = compressor->get_options();
  pressio_data lower_bound = vector_to_owning_pressio_data<double>({0.0});
  pressio_data upper_bound = vector_to_owning_pressio_data<double>({0.1});
  pressio_data guess = vector_to_owning_pressio_data<double>({1e-5});
  options.set("opt:search", "fraz"); //binary search is non-monotonic for this input using SZ_REL
  options.set("opt:compressor", "sz");
  options.set("opt:inputs", std::vector<std::string>{"sz:rel_err_bound"});
  options.set("opt:lower_bound", lower_bound);
  options.set("opt:upper_bound", upper_bound);
  options.set("opt:target", 400.0);
  options.set("opt:local_rel_tolerance", 0.1);
  options.set("opt:global_rel_tolerance", 0.1);
  options.set("opt:max_iterations", static_cast<unsigned int>(100));
  options.set("opt:output", "size:compression_ratio");
  options.set("opt:do_decompress", 0);
  options.set("opt:search_metrics", "progress_printer");
  options.set("opt:prediction", guess);
  options.set("sz:error_bound_mode", REL);
  compressor->set_options(options);
  options = compressor->get_options();
  std::cout << "options:" << std::endl << options << std::endl;

  size_t dims[] = {500,500,100};
  auto input_data = pressio_data_new_move(pressio_float_dtype, make_data(), 3, dims, pressio_data_libc_free_fn, nullptr);
  auto compressed = pressio_data_new_empty(pressio_byte_dtype, 0, 0);
  auto decompressed = pressio_data_new_owning(pressio_double_dtype, 3, dims);
  
  if(compressor->compress(input_data, compressed)) {
    std::cerr << compressor->error_msg() << std::endl;
    return compressor->error_code();
  }

  auto metrics_results = compressor->get_metrics_results();
  std::cout << "Metrics Results:" << std::endl << metrics_results << std::endl;

  pressio_data_free(input_data);
  pressio_data_free(compressed);
  pressio_data_free(decompressed);
  return 0;
}
