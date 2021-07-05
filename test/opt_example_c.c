#include <dataCompression.h>
#include <libpressio.h>
#include <pressio_search_defines.h>
#include <stdio.h>
#include <sz.h>
#include <mpi.h>

//create a data-buffer for testing
float* make_data() {
  size_t idx = 0;
  float* data = (float*)malloc(sizeof(float)*500*500*100);
  for (int i = 0; i < 500; ++i) {
    for (int j = 0; j < 500; ++j) {
      for (int k = 0; k < 100; ++k) {
        data[idx] = (float)(i*i + 2*j - k);
        idx++;
      }
    }
  }
  return data;
}


int main(int argc, char *argv[])
{
  // initialize MPI for use with threads to spread the search across nodes
  int rank, size, thread_provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &thread_provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if(thread_provided != MPI_THREAD_MULTIPLE) {
    if(rank == 0){
      printf("insufficient thread support from MPI\n");
    }
    MPI_Abort(MPI_COMM_WORLD, 3);
  }


  // create a opt meta-compressor object; this will preform the search for the appropriate compressor settings
  struct pressio* library = pressio_instance();
  struct pressio_compressor* compressor = pressio_get_compressor(library, "opt");
  struct pressio_options* configuration = pressio_compressor_get_configuration(compressor);
  if(rank == 0) {
    // print the configuration of the compressor
    char* configuration_str = pressio_options_to_string(configuration);
    printf("configuration:\n%s\n", configuration_str);
    free(configuration_str);
  }

  // create a metrics object that using a multi-objective search
  // We are loading the size, time, and error_stat modules, the composite
  // module used below is always loaded implicitly
  const char* metrics_ids[] = { "size", "time", "error_stat" };
  struct pressio_metrics* metrics = pressio_new_metrics(library, metrics_ids, 3);
  pressio_compressor_set_metrics(compressor, metrics);
  struct pressio_options* metric_options = pressio_compressor_metrics_get_options(compressor);

  pressio_compressor_metrics_set_options(compressor, metric_options);
  pressio_options_free(metric_options);


  /*
   * now configure the compressor, we are going to use a few meta search methods
   *
   * + guess_first - to avoid searching if our prediction for the correct parameters is "correct"
   * + dist_gridsearch - to distribute the search space across the cluster
   * + fraz - a robust search method
   *
   * We will print the search progress using the progress_printer search metrics
   *
   * And we will search using the sz error bounded compressor's REL (value range
   * relative error bound mode.
   *
   * We will do this in a few stages:
   *  1. Setup the compressor and search trees
   *  2. Describe the search space
   *  3. Describe the outputs
   *  4. Configure early termination thresholds
   *  5. Configure the specific options unique to dist_gridsearch, fraz, and sz
   */
  // 1. Setup the compressor and search trees
  struct pressio_options* options = pressio_compressor_get_options(compressor);
  pressio_options_set_string(options, "opt:compressor", "sz");
  pressio_options_set_strings(options, "composite:plugins", sizeof(metrics_ids)/sizeof(metrics_ids[0]), metrics_ids);
  pressio_options_set_string(options, "sz:metric", "composite");
  pressio_options_set_string(options, "opt:search", "guess_first");
  pressio_options_set_string(options, "guess_first:search", "dist_gridsearch");
  pressio_options_set_string(options, "dist_gridsearch:search", "fraz");
  pressio_options_set_string(options, "opt:search_metrics", "progress_printer");
  /*
   * here we use the composite metrics's ability to combine metrics from other modules
   * to compute a hybrid metric.
   *
   * Since we want to find the best compression_ratio subject to a psnr threshold of 65,
   * we return -inf if we are below the threshold and the compression_ratio if we are above it.
   *
   * The string "objective" returned here is the key we will use to recall the metric later
   * see where we configure outputs below
   *
   * Key options summarized here, please refer to the pressio_search options docs for more information
   */
  const char* lua_scripts = 
    "local cr = metrics['size:compression_ratio'];\n"
    "local psnr = metrics['error_stat:psnr'];\n"
    "local threshold = 65.0;\n"
    "local objective = 0;\n"
    "if psnr < threshold then\n"
    "  objective = -math.huge;\n"
    "else\n"
    "  objective = cr;\n"
    "end\n"
    "return \"objective\", objective;\n";

  pressio_options_set_strings(options, "composite:scripts", 1, &lua_scripts);

  // 2. Describe the search space
  const char* inputs[] = {"sz:rel_err_bound"};
  double lower_bound[] = {0.0};
  double upper_bound[] = {0.1};
  size_t bound_dims[] = {1};
  struct pressio_data* lower_bound_data = pressio_data_new_nonowning(pressio_double_dtype, lower_bound, 1, bound_dims);
  struct pressio_data* upper_bound_data = pressio_data_new_nonowning(pressio_double_dtype, upper_bound, 1, bound_dims);

  pressio_options_set_strings(options, "opt:inputs", 1, inputs);
  pressio_options_set_data(options, "opt:lower_bound", lower_bound_data);
  pressio_options_set_data(options, "opt:upper_bound", upper_bound_data);

  // 3. Define the outputs
  //  NOTE: some search methods take advantage of multiple outputs to guide the search for the overall
  //  objective (shown in the first item of outputs).  We could set output to just the first item only
  //  and pass 1 instead of 3 and would likely get the same result, albeit more slowly.
  //
  //  NOTE: for the definition of composite:objective refer to lua_script above.  We could avoid
  //  using lua by passing a native module instead such as size:compression_ratio; We could also 
  //  use the "external" metrics module to run a script that gathers the metrics
  const char* outputs[] = {"composite:objective", "size:compression_ratio", "error_stat:psnr"};
  pressio_options_set_strings(options, "opt:output", 3, outputs);
  //  NOTE: some metrics require the decompressed data in-order to be
  //  calculated.  Since we are using error_stat:psnr which needs the
  //  decompressed data, we need to preform decompression so we set this to 1.
  //  If we were just using compression_ratio which doesn't, we could set this to 0
  pressio_options_set_integer(options, "opt:do_decompress", 1);
  pressio_options_set_uinteger(options, "opt:objective_mode", pressio_search_mode_max);

  //4. Configure early termination thresholds and runtime constraints
  pressio_options_set_double(options, "opt:target", 40000.0); 
  pressio_options_set_double( options, "opt:local_rel_tolerance", 0.1);
  pressio_options_set_uinteger(options, "opt:max_iterations", 100u);
  double guess[] = {1e-5};
  struct pressio_data* guess_data = pressio_data_new_nonowning(pressio_double_dtype, guess, 1, bound_dims);
  pressio_options_set_data(options, "opt:prediction", guess_data);

  //configure dist_gridsearch's unique parameters
  //  NOTE: to limit the number of MPI processes used for the search, pass a sub communicator instead of MPI_COMM_WORLD
  double overlap_percentage[] = {.1};
  size_t num_bins[] = {(size_t)((size == 1) ? 1 : (size -1))};
  struct pressio_data* num_bins_data = pressio_data_new_nonowning(pressio_uint64_dtype, num_bins, 1, bound_dims);
  struct pressio_data* overlap_percentage_data = pressio_data_new_nonowning(pressio_double_dtype, num_bins, 1, bound_dims);
  pressio_options_set_data(options, "dist_gridsearch:num_bins", num_bins_data);
  pressio_options_set_data(options, "dist_gridsearch:overlap_percentage", overlap_percentage_data);
  pressio_options_set_userptr(options, "distributed:comm", (void*)MPI_COMM_WORLD);

  //configure fraz's unique fixed parameters
  //  NOTE: some search methods can use multiple threads in addition to multiple processes
  //  These parameters are ignored if the compressor or MPI implementation do not support it
  //  You can query if a compressor supports multiple threads by calling pressio_compressor_get_configuration
  //  and reviewing the "pressio:thread_safe" configuration parameter.
  pressio_options_set_uinteger(options, "fraz:nthreads", 4u);

  //configure sz error bounded compressor fixed parameters
  pressio_options_set_integer(options, "sz:error_bound_mode", REL);

  //apply the compressor and search options and print them out
  if(pressio_compressor_set_options(compressor, options)) {
    printf("%s\n", pressio_compressor_error_msg(compressor));
    exit(pressio_compressor_error_code(compressor));
  }
  pressio_options_free(options);
  if(rank == 0){
    options = pressio_compressor_get_options(compressor);
    char* options_str = pressio_options_to_string(options);
    printf("options:\n%s\n", options_str);
    free(options_str);
    pressio_options_free(options);
  }


  //prepare the input, decompressed, and output buffers
  size_t dims[] = {500,500,100};
  struct pressio_data* input_data = pressio_data_new_move(pressio_float_dtype, make_data(), 3, dims, pressio_data_libc_free_fn, NULL);
  struct pressio_data* compressed = pressio_data_new_empty(pressio_byte_dtype, 0, 0);
  struct pressio_data* decompressed = pressio_data_new_owning(pressio_float_dtype, 3, dims);
  
  //compress the data and run the search
  if(pressio_compressor_compress(compressor, input_data, compressed)) {
    printf("%s\n",pressio_compressor_error_msg(compressor));
    return pressio_compressor_error_code(compressor);
  }

  //the search metrics have the best value we found so far
  if(rank == 0) {
    struct pressio_options* metrics_results = pressio_compressor_get_metrics_results(compressor);
    char* metrics_results_str = pressio_options_to_string(metrics_results);
    printf("Metrics Results:\n%s\n", metrics_results_str);
    free(metrics_results_str);
    pressio_options_free(metrics_results);
  }

  //free all objects we allocated
  pressio_data_free(input_data);
  pressio_data_free(compressed);
  pressio_data_free(decompressed);
  pressio_data_free(lower_bound_data);
  pressio_data_free(upper_bound_data);
  pressio_data_free(guess_data);
  pressio_data_free(num_bins_data);
  pressio_data_free(overlap_percentage_data);
  pressio_compressor_release(compressor);
  pressio_metrics_free(metrics);
  pressio_release(library);

  MPI_Finalize();
  return 0;
}
