#include <iostream>
#include <libpressio.h>
#include <libpressio_ext/cpp/printers.h>

int main(int argc, char *argv[])
{
  auto library = pressio_instance();  
  auto compressor = pressio_get_compressor(library, "opt");

  auto configuration = pressio_compressor_get_configuration(compressor);
  std::cout << "configuration:" << std::endl << *configuration << std::endl;

  auto options = pressio_compressor_get_options(compressor);
  std::cout << "options:" << std::endl << *options << std::endl;



  
  return 0;
}
