#ifndef PRESSIO_DATA_UTILITIES_H
#define PRESSIO_DATA_UTILITIES_H
#include <vector>
#include <type_traits>
#include <libpressio_ext/cpp/data.h>

/** \file 
 *  \brief functions that convert between standard types and pressio_data structures
 */

namespace {
  template <class T>
    constexpr pressio_dtype type_to_dtype() {
      return (std::is_same<T, double>::value ? pressio_double_dtype :
          std::is_same<T, float>::value ? pressio_float_dtype :
          std::is_same<T, int64_t>::value ? pressio_int64_dtype :
          std::is_same<T, int32_t>::value ? pressio_int32_dtype :
          std::is_same<T, int16_t>::value ? pressio_int16_dtype :
          std::is_same<T, int8_t>::value ? pressio_int8_dtype :
          std::is_same<T, uint64_t>::value ? pressio_uint64_dtype :
          std::is_same<T, uint32_t>::value ? pressio_uint32_dtype :
          std::is_same<T, uint16_t>::value ? pressio_uint16_dtype :
          std::is_same<T, uint8_t>::value ? pressio_uint8_dtype :
          pressio_byte_dtype
          );
    }
}

/**
 * converts a pressio_data structure to a std::vector of the template type
 *
 * \param[in] data the data to convert
 * \returns a new vector
 */
template <class T>
std::vector<T> pressio_data_to_vector(pressio_data const& data) {
  return std::vector<T>(static_cast<T*>(data.data()), static_cast<T*>(data.data()) + data.num_elements());
}


/**
 * converts a std::vector of template type to a pressio_data structure
 *
 * \param[in] vec the data to convert
 * \returns a pressio_data structure
 */
template <class T>
pressio_data vector_to_owning_pressio_data(std::vector<T> const& vec) {
  return pressio_data::copy(type_to_dtype<T>(), vec.data(), {vec.size()});
}


#endif /* end of include guard: PRESSIO_DATA_UTILITIES_H_TGEL8B1F */
