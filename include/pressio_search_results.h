#ifndef PRESSIO_SEARCH_RESULTS_H
#define PRESSIO_SEARCH_RESULTS_H

#include <vector>
#include <string>
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
  using output_type = std::vector<double>;
  /** input to the best iteration*/
  input_type inputs{};
  /** the multi-dimensional output of the search.  the first entry should represent the summary or primary objective */
  output_type output{};


  /** status of the search, 0 on success, <0 on warning, >0 on error */
  int status = 0;
  /** human-readable message describing the status on warnings or error */
  std::string msg{};

};

#endif /* end of include guard: PRESSIO_SEARCH_RESULTS_H */
