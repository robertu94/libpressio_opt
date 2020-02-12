#include <stdexcept>

class pressio_search_exception : public std::runtime_error {
  using runtime_error::runtime_error;
};
