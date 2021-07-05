#include <iostream>
#include <limits>
#include <libpressio.h>
#include <libpressio_ext/cpp/libpressio.h>
#include <libpressio_ext/io/posix.h>
#include <pressio_search_defines.h>
#include <sz.h>
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
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if(thread_provided != MPI_THREAD_MULTIPLE) {
    if(rank == 0){
      std::cout << "insufficient thread support from MPI" << std::endl;
    }
    MPI_Abort(MPI_COMM_WORLD, 3);
  }


  pressio library;
  std::string metrics_ids[] = {"size", "time", "error_stat"};
  pressio_metrics metrics = library.get_metrics(std::begin(metrics_ids), std::end(metrics_ids));
  pressio_options metric_options = metrics->get_options();
  std::vector<std::string> lua_scripts { R"lua(
    local cr = metrics['size:compression_ratio'];
    local psnr = metrics['error_stat:psnr'];
    local threshold = 65.0;
    local objective = 0;
    if (psnr ~= nil and psnr < threshold) then
      objective = -math.huge;
    else
      objective = cr;
    end
    return "objective", objective
  )lua"};


  auto compressor = library.get_compressor("opt");
  auto configuration = compressor->get_configuration();
  if(rank == 0) {
    std::cout << "configuration:" << std::endl << configuration << std::endl;
  }

  auto const inputs = std::vector<std::string>{"sz:rel_err_bound"};
  auto const outputs = std::vector<std::string>{"composite:objective", "size:compression_ratio", "error_stat:psnr"};
  auto const headers = [&]()
  {
    std::vector<std::string> headers;
    headers.insert(headers.end(), inputs.begin(), inputs.end());
    headers.insert(headers.end(), outputs.begin(), outputs.end());
    return headers;
  }();

  auto options = compressor->get_options();
  pressio_data lower_bound{0.0};
  pressio_data upper_bound{0.1};
  pressio_data guess = pressio_data{1e-5};
  options.set("fraz:nthreads", 4u);
  options.set("opt:search", "dist_gridsearch");
  options.set("dist_gridsearch:search", "fraz");
  options.set("dist_gridsearch:num_bins", pressio_data{(size == 1) ? 1 : (size -1)});
  options.set("dist_gridsearch:overlap_percentage", pressio_data{.1,});
  options.set("distributed:comm", (void*)MPI_COMM_WORLD);
  options.set("opt:compressor", "sz");
  options.set("opt:inputs", inputs);
  options.set("opt:lower_bound", lower_bound);
  options.set("opt:upper_bound", upper_bound);
  options.set("opt:target", 40000.0);
  options.set("opt:local_rel_tolerance", 0.1);
  options.set("opt:global_rel_tolerance", 0.1);
  options.set("opt:max_iterations", 100u);
  options.set("opt:output", outputs);
  options.set("opt:search_metrics", "composite_search");
  options.set("composite_search:search_metrics", std::vector<std::string>{"progress_printer", "record_search"});
  options.set("io:path", "/tmp/trace.csv");
  options.set("csv:headers", headers);
  options.set("opt:prediction", guess);
  options.set("opt:do_decompress", 1);
  options.set("opt:objective_mode", (unsigned int)pressio_search_mode_max);
  options.set("sz:error_bound_mode", REL);
  options.set("sz:metric", "composite");
  options.set("opt:metric", "composite");
  options.set("composite:plugins", std::vector<std::string>{"time", "size", "error_stat"});
  options.set("composite:names", std::vector<std::string>{"time", "size", "error_stat"});
  options.set("composite:scripts", lua_scripts);
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
  auto decompressed = pressio_data_new_owning(pressio_float_dtype, 3, dims);
  
  if(compressor->compress(input_data, compressed)) {
    std::cerr << compressor->error_msg() << std::endl;
    return compressor->error_code();
  }
  if(compressor->decompress(compressed, decompressed)) {
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
