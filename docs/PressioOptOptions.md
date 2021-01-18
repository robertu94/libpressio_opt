# Configuration Options {#optoptions}

This page summarizes the options used by the opt meta compressor.

Here is a list of the current search algorithms:



+ Binary Search (binary) -- simple binary search.
+ Random (random) -- guess points randomly.
+ Guess (guess) -- guess a specific point.
+ FRaZ (fraz) -- a robust searching method.
+ Guess First (guess_first) -- guess a specific point, then fall back to a search.
+ Distributed Grid Search (dist\_gridsearch) -- distribute a search space and then search it.

## Common Options

The opt meta-compressor has a search algorithm.  The meta-compressor itself has some options which affect all searches:

| option name               | type                                         | description |
|---------------------------|----------------------------------------------|------------------------|
|`opt:compressor`           | string                                       | the compressor to use |
|`opt:inputs`               | string[]                                     | list of input metrics |
|`opt:output`               | string[]                                     | the name of the output parameters |
|`opt:do_decompress`        | int                                          | 0 if decompressed is not required, 1 otherwise |
|`opt:search_metrics`       | string                                       | the name of a search_metrics module to load. see below |

Additionally, there are several options which are common to each of the search algorithms.

| option name                | type                                                            | description                                                                                                         |
|----------------------------|-----------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| `distributed:mpi_comm`     | `MPI_Comm`                                                      | `MPI_Comm` passed to libdistributed                                                                                 |
| `opt:lower_bound`          | `pressio_data` containing double[`n_inputs`]                    | the lower bound to search                                                                                           |
| `opt:upper_bound`          | `pressio_data` containing double[`n_inputs`]                    | the upper bound to search                                                                                           |
| `opt:target`               | double                                                          | the target the user wants to achieve, meaning dependson opt:objective_mode                                          |
| `opt:local_rel_tolerance`  | double                                                          | a parameter that controls how likely the searcher is to refine vs explore                                           |
| `opt:global_rel_tolerance` | double                                                          | a paremeter that specifies how close we must be to the target to early terminate if `opt:objective_mode==target`    |
| `opt:max_iterations`       | unsigned int                                                    | the maximum number of iterations _per task_                                                                         |
| `opt:prediction`           | `pressio_data` containing double[`n_inputs`]                    | the prediction to use                                                                                               |
| `opt:evaluations`          | `pressio_data` containing double[`n_inputs` + 1, n_evaluations] | a set of N evaluations preformed early                                                                               |
| `opt:objective_mode`       | unsigned int                                                    | the type of search to preform (min -- find a min, max -- find a max, target -- get as close to a target as possible |

Each Searcher has some properties.  Their meanings are explained below:

| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | Can the search method support multiple objectives? Yes -- tunes search using all metrics, Composite -- supported if the user provides a combining function,  no -- not guided based on outputs   |
| Multi-Dimension   | Can the number of inputs be greater than 1? |
| Distributed       | Does this searcher itself distribute the search over several nodes? |
| Multithreaded     | Does this searcher itself create and use threads? |

**Note:** Searchers that are not multi-threaded or distributed can be called from multi-threaded or distributed searchers.


## Searcher Specific Options

### Binary Search (binary)

Performs a binary search for the error bound.

| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | composite               |
| Multi-Dimension   | false                   |
| Distributed       | false                   |
| Multithreaded     | false                   |


Binary Search supports the following common options

+ `opt:prediction`
+ `opt:lower_bound`
+ `opt:upper_bound`


### Random (random)

Random search just randomly evaluates some points.

| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | composite               |
| Multi-Dimension   | true                    |
| Distributed       | true                    |
| Multithreaded     | false                   |


Random search supports the following common options:

+ `distributed:mpi_comm`
+ `opt:lower_bound`
+ `opt:max_iterations`
+ `opt:max_seconds`
+ `opt:objective_mode`
+ `opt:prediction`
+ `opt:target`
+ `opt:upper_bound`

Random search also supports the following common options:

|  option name     | type         | description                                 |  
|------------------|--------------|---------------------------------------------|  
| `random:seed`    | `optional<unsigned int>` | the seed to use, if the optional is empty, a random seed is used |

### Guess (guess)

Guess search simply guesses a specific predicted value.

| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | composite               |
| Multi-Dimension   | true                    |
| Multithreaded     | false                   |
| Distributed       | false                   |


Guess supports the following common options: 

+ `opt:prediction`

### FRaZ (fraz)

A robust search method roughly based on [dlib's `find_global_min` routine](http://blog.dlib.net/2017/12/a-global-optimization-algorithm-worth.html).
Find details about this algorithm in the following papers:

+ Underwood, Robert, et al. "FRaZ: A Generic High-Fidelity Fixed-Ratio Lossy Compression Framework for Scientific Floating-point Data." International Symposium Parallel and Distributed Processing (IPDPS). 2020.


| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | composite               |
| Multi-Dimension   | true                    |
| Multithreaded     | true                    |
| Distributed       | false                   |

FRaZ support the following kinds of common options:

+ `opt:global_rel_tolerance`
+ `opt:local_rel_tolerance`
+ `opt:lower_bound`
+ `opt:max_iterations`
+ `opt:max_seconds`
+ `opt:objective_mode`
+ `opt:prediction`
+ `opt:target`
+ `opt:upper_bound`
+ `opt:evaluations`

FRaZ also supports the following specific options:

|  option name     | type         | description                                 |  
|------------------|--------------|---------------------------------------------|  
|  `fraz:nthreads` | unsigned int | the number of threads to use in the search  |


## Meta Searcher Specific Options

### Guess First (guess_first)

Attempts a guess first, then falls back to another search method.

| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | composite               |
| Multi-Dimension   | true                    |
| Multithreaded     | false                   |
| Distributed       | false                   |

Guess First supports the following common options:

`opt:target`
`opt:objective_mode`
`opt:global_rel_tolerance`
`opt:prediction`
`opt:target`

|  option name                         | type         | description                                 |  
|--------------------------------------|--------------|---------------------------------------------|  
| `guess_first:search`                 | string       | the search method use if the guess fails |


### Distributed Grid Search (dist\_gridsearch)

Splits the domain into a number of bins and executes a subsearch on each in separate tasks in distributed memory.

| Searcher Property | Value                   |
|-------------------|-------------------------|
| Multi-Objective   | composite               |
| Multi-Dimension   | true                    |
| Multithreaded     | false                   |
| Distributed       | true                    |


Distributed grid search supports the following common options:

+ `distributed:mpi_comm`
+ `opt:lower_bound`
+ `opt:upper_bound`
+ `opt:target`
+ `opt:global_rel_tolerance`
+ `opt:objective_mode`

Distributed grid search as the following unique options:

|  option name                         | type         | description                                 |  
|--------------------------------------|--------------|---------------------------------------------|  
| `dist_gridsearch:search`             | string                                                 | the search method to use |
| `dist_gridsearch:num_bins`           | `pressio_data` containing unsigned int[num_dimensions] | the number of search bins to divide each dimension into |
| `dist_gridsearch:overlap_percentage` | `pressio_data` containing double[]                     | the amount to overlap each bin |
