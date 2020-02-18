#ifdef __cplusplus
extern "C" { 
#endif
#ifndef PRESSIO_SEARCH_DEFINES_H
#define PRESSIO_SEARCH_DEFINES_H
#include <stddef.h>

/**
 * Callback to compute the objective from multiple objectives
 *
 * \param[in] data the objectives computed
 * \param[in] size the number of objectives passed
 * \param[in] user_data auxiliary constants provided by the user may include constraints, etc...
 * \returns the computed objective
 */
typedef double (*pressio_opt_multiobjective_fn)(const double* data, const size_t size, const void* user_data) ;

/**
 * Standard call that invokes a std::function<double(std::vector<double> const&)>
 *
 * \param[in] data the objectives computed
 * \param[in] size the number of objectives passed
 * \param[in] stdfn pointer to a std::function<double(std::vector<double> const&)>

 * \returns the computed objective
 */
double pressio_opt_multiobjective_stdfn(const double* data,  const size_t size, const void* stdfn);
/**
 * Standard call that returns the first element from data allowing single objective optimization 
 * to be treated the same way as multi-objective optimization
 *
 * \param[in] data the objectives computed
 * \param[in] size the number of objectives passed
 * \param[in] ignored not used by this method
 * \returns the first element from data
 */
double pressio_opt_multiobjective_first(const double* data,  const size_t size, const void* ignored);

/**
 * describes different methodologies for searching
 */
enum pressio_search_mode {
  /**
   * search for the value which gets within some tolerance of the objective
   * the search may terminate early if such a value is found
   */
  pressio_search_mode_target = 1,
  /**
   * search for the value which minimizes the objective
   */
  pressio_search_mode_min = 2,
  /**
   * search for the value which maximizes the objective
   */
  pressio_search_mode_max = 4,
};

#endif /* end of include guard: PRESSIO_SEARCH_DEFINES_H */
#ifdef __cplusplus
}
#endif
