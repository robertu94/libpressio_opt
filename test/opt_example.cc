#include <iostream>
#include <libpressio.h>
#include <libpressio_ext/io/posix.h>
#include <libpressio_ext/cpp/printers.h>

int main(int argc, char *argv[])
{
  auto library = pressio_instance();  
  auto compressor = pressio_get_compressor(library, "opt");

  auto configuration = pressio_compressor_get_configuration(compressor);
  std::cout << "configuration:" << std::endl << *configuration << std::endl;

  auto options = pressio_compressor_get_options(compressor);
  std::cout << "options:" << std::endl << *options << std::endl;

  pressio_compressor_set_options(compressor, options);

  size_t dims[] = {500,500,100};
  auto description = pressio_data_new_owning(pressio_double_dtype, 3, dims);
  auto input_data = pressio_io_data_path_read(description, "/home/runderwood/git/datasets/hurricane/100x500x500/CLOUDf48.bin.f32");
  auto compressed = pressio_data_new_empty(pressio_byte_dtype, 0, 0);
  auto decompressed = pressio_data_new_owning(pressio_double_dtype, 3, dims);
  
  pressio_compressor_compress(compressor, input_data, compressed);
  pressio_compressor_decompress(compressor, compressed, decompressed);

  pressio_options_free(options);
  pressio_data_free(input_data);
  pressio_data_free(compressed);
  pressio_data_free(decompressed);
  pressio_compressor_release(compressor);
  pressio_release(library);
  return 0;
}
