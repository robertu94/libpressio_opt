#include <iostream>
#include <limits>
#include <libpressio.h>
#include <libpressio_ext/cpp/libpressio.h>
#include <libpressio_ext/io/posix.h>
#include <libpressio_opt_ext/impl/pressio_data_utilities.h>
#include <pressio_search_defines.h>
#include <sz/sz.h>
#include <mpi.h>

float* make_data() {
  size_t idx = 0;
  auto data = static_cast<float*>(malloc(sizeof(float)*500*500*100));
  for (int i = 0; i < 500; ++i) {
    for (int j = 0; j < 500; ++j) {
      for (int k = 0; k < 100; ++k) {
        data[idx] = i*i + 2*j - k;
        idx++;
      }
    }
  }
  return data;
}


int main(int argc, char *argv[])
{
  int rank, size, thread_provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &thread_provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if(thread_provided != MPI_THREAD_MULTIPLE) {
    if(rank == 0){
      std::cout << "insufficient thread support from MPI" << std::endl;
    }
    MPI_Abort(MPI_COMM_WORLD, 3);
  }


  pressio library;
  std::string metrics_ids[] = {"size", "time", "error_stat"};
  pressio_metrics metrics = library.get_metrics(std::begin(metrics_ids), std::end(metrics_ids));
  auto compressor = library.get_compressor("opt");
  auto configuration = compressor->get_configuration();
  compressor->set_metrics(metrics);
  if(rank == 0) {
    std::cout << "configuration:" << std::endl << configuration << std::endl;
  }

  double psnr_threshold = 65.0;
  std::function<double(std::vector<double>const&)> objective = [=](std::vector<double> const& results) {
    double cr = results.at(0);
    double psnr = results.at(1);
    if(psnr < psnr_threshold) return std::numeric_limits<double>::lowest();
    return cr;
  };

  auto options = compressor->get_options();
  pressio_data lower_bound{0.0};
  pressio_data upper_bound{0.1};
  pressio_data guess = vector_to_owning_pressio_data<double>({1e-5});
  options.set("opt:search", "dist_gridsearch"); //binary search is non-monotonic for this input using SZ_REL
  options.set("dist_gridsearch:search", "fraz"); //binary search is non-monotonic for this input using SZ_REL
  options.set("dist_gridsearch:num_bins", pressio_data{5ul,});
  options.set("dist_gridsearch:overlap_percentage", pressio_data{.1,});
  options.set("dist_gridsearch:comm", (void*)MPI_COMM_WORLD);
  options.set("fraz:nthreads", 4u);
  options.set("opt:compressor", "sz");
  options.set("opt:inputs", std::vector<std::string>{"sz:rel_err_bound"});
  options.set("opt:lower_bound", lower_bound);
  options.set("opt:upper_bound", upper_bound);
  options.set("opt:target", 400.0);
  options.set("opt:local_rel_tolerance", 0.1);
  options.set("opt:global_rel_tolerance", 0.1);
  options.set("opt:max_iterations", 100u);
  options.set("opt:output", std::vector<std::string>{"size:compression_ratio", "error_stat:psnr"});
  options.set("opt:do_decompress", 0);
  options.set("opt:search_metrics", "progress_printer");
  options.set("opt:prediction", guess);
  options.set("opt:do_decompress", 1);
  options.set("opt:objective_fn", (void*)pressio_opt_multiobjective_stdfn);
  options.set("opt:objective_data", (void*)&objective);
  options.set("opt:objective_mode", (unsigned int)pressio_search_mode_max);
  options.set("sz:error_bound_mode", REL);
  if(compressor->set_options(options)) {
    std::cout << compressor->error_msg() << std::endl;
    exit(compressor->error_code());
  }
  options = compressor->get_options();
  if(rank == 0){
    std::cout << "options:" << std::endl << options << std::endl;
  }


  size_t dims[] = {500,500,100};
  auto input_data = pressio_data_new_move(pressio_float_dtype, make_data(), 3, dims, pressio_data_libc_free_fn, nullptr);
  auto compressed = pressio_data_new_empty(pressio_byte_dtype, 0, 0);
  auto decompressed = pressio_data_new_owning(pressio_double_dtype, 3, dims);
  
  if(compressor->compress(input_data, compressed)) {
    std::cerr << compressor->error_msg() << std::endl;
    return compressor->error_code();
  }

  auto metrics_results = compressor->get_metrics_results();
  if(rank == 0) {
    std::cout << "Metrics Results:" << std::endl << metrics_results << std::endl;
  }

  pressio_data_free(input_data);
  pressio_data_free(compressed);
  pressio_data_free(decompressed);
  MPI_Finalize();
  return 0;
}
