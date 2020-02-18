#ifndef PRESSIO_DATA_UTILITIES_H
#define PRESSIO_DATA_UTILITIES_H
#include <vector>
#include <type_traits>
#include <libpressio_ext/cpp/data.h>
#include <libpressio_ext/cpp/dtype.h>

/** \file 
 *  \brief functions that convert between standard types and pressio_data structures
 */

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
  return pressio_data::copy(pressio_dtype_from_type<T>(), vec.data(), {vec.size()});
}


#endif /* end of include guard: PRESSIO_DATA_UTILITIES_H_TGEL8B1F */
