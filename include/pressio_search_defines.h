#ifdef __cplusplus
extern "C" { 
#endif
#ifndef PRESSIO_SEARCH_DEFINES_H
#define PRESSIO_SEARCH_DEFINES_H

/** \file
 *  \brief definitions of constants needed to interact with libpressio_opt from C
 */


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
