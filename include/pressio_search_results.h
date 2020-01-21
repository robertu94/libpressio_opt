#ifndef PRESSIO_SEARCH_RESULTS_H
#define PRESSIO_SEARCH_RESULTS_H

#include <vector>
/**
 * \file
 * \brief provides the definitions used for search_results
 */

/** class to represent a particular search result */
struct pressio_search_results {
  /** type of a single element of the input_type*/
  using input_element_type = double;
  /** type of the input_type*/
  using input_type = std::vector<double>;
  /** type of the output_type*/
  using output_type = double;
  /** input to the best iteration*/
  input_type inputs;
  /** result of the best iteration*/
  output_type output;
};

#endif /* end of include guard: PRESSIO_SEARCH_RESULTS_H */
