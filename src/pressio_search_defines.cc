#include "pressio_search_defines.h"
#include <functional>

extern "C" {
double pressio_opt_multiobjective_stdfn(const double* data,  const size_t size, const void* stdfn) {
  auto fn = static_cast<const std::function<double(std::vector<double> const&)>*>(stdfn);
  std::vector<double> vec(data, data+size);
  return (*fn)(vec);
}


double pressio_opt_multiobjective_first(const double* data, const size_t size, const void*) {
  return data[0];
}

}
