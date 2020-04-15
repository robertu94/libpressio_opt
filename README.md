# LibPressioOpt

LibPressioOpt provides a plugin for libpressio that provides optimization routines to configure compressors.

## Using LibPressioOpt

An example of using LibPressioOpt is under development.

## Getting Started

LibPressioOpt provides three new major features on top of LibPressio:


+ the `opt` meta compressor which allows for searching for the optimal configuration of the compressor
+ `pressio_search` modules which allow for searching for an optimal set of configuration of parameters
+ `pressio_search_metrics` modules which compute properties of the search process itself


## Dependencies

+ `cmake` version `3.13` or later
+ either:
  + `gcc-8.3.0` or later
  + `clang-9.0.0` or later
+ LibDistributed version 0.0.6 or later
+ LibPressio version 0.34.4 or later
+ An MPI implementation supporting MPI-3 or later.  Tested on OpenMPI 4.0.2


## Building and Installing LibPressioOpt

LibPressioOpt uses CMake to configure build options.  See CMake documentation to see how to configure options

+ `CMAKE_INSTALL_PREFIX` - install the library to a local directory prefix
+ `BUILD_DOCS` - build the project documentation
+ `BUILD_TESTING` - build the test cases

```bash
BUILD_DIR=build
mkdir $BUILD_DIR
cd $BUILD_DIR
cmake ..
make
make test
make install
```

To build the documentation:


```bash
BUILD_DIR=build
mkdir $BUILD_DIR
cd $BUILD_DIR
cmake .. -DBUILD_DOCS=ON
make docs
# the html docs can be found in $BUILD_DIR/html/index.html
# the man pages can be found in $BUILD_DIR/man/
```


## Stability

As of version 1.0.0, LibPressioOpt will follow the following API stability guidelines:

+ The functions defined in files in `./include` are to considered stable
+ The functions defined in files or its subdirectories in `./include/libpressio_opt_ext/` considered unstable.

Stable means:

+ New APIs may be introduced with the increase of the minor version number.
+ APIs may gain additional overloads for C++ compatible interfaces with an increase in the minor version number.
+ An API may change the number or type of parameters with an increase in the major version number.
+ An API may be removed with the change of the major version number

Unstable means:

+ The API may change for any reason with the increase of the minor version number

Additionally, the performance of functions, memory usage patterns may change for both stable and unstable code with the increase of the patch version.


## Bug Reports

Please files bugs to the Github Issues page on the robertu94 github repository.

Please read this post on [how to file a good bug report](https://codingnest.com/how-to-file-a-good-bug-report/).  After reading this post, please provide the following information specific to LibPressioOpt:

+ Your OS version and distribution information, usually this can be found in `/etc/os-release`
+ the output of `cmake -L $BUILD_DIR`
+ the version of each of LibPressioOpts's dependencies listed in the README that you have installed. Where possible, please provide the commit hashes.

