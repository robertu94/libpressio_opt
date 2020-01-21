cmake_minimum_required(VERSION 3.12)
project(libpressio_opt VERSION "0.0.2" LANGUAGES C CXX)

#correct was to set a default build type
# https://blog.kitware.com/cmake-and-the-default-build-type/
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type was set. Setting build type to ${default_build_type}.")
  set(CMAKE_BUILD_TYPE ${default_build_type} CACHE 
    STRING "Choose the type to build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

include(CTest)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(BUILD_SHARED_LIBS "build libraries as shared" ON)

set(default_cxx_standard 17)
if(NOT LIBPRESSIO_OPT_CXX_VERSION)
  set(LIBPRESSIO_OPT_CXX_VERSION ${default_cxx_standard} CACHE 
    STRING "Which version of the cxx standard to expect" FORCE)
  set_property(CACHE LIBPRESSIO_OPT_CXX_VERSION PROPERTY STRINGS 17)
endif()

include(GNUInstallDirs)

find_package(MPI COMPONENTS CXX)
find_package(LibPressio REQUIRED)
find_package(SZ REQUIRED)
find_package(zfp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(MGARD REQUIRED)
find_package(fpzip REQUIRED)
find_package(MPI REQUIRED COMPONENTS CXX)
find_package(LibDistributed REQUIRED)
find_package(PkgConfig)
pkg_search_module(ZSTD IMPORTED_TARGET GLOBAL libzstd)
pkg_search_module(FFTW3 IMPORTED_TARGET GLOBAL fftw3)
pkg_search_module(BLOSC IMPORTED_TARGET GLOBAL blosc)
pkg_search_module(Magick++ IMPORTED_TARGET GLOBAL Magick++)

add_library(libpressio_opt
  #core features
    src/pressio_opt.cc
    src/pressio_search.cc
    src/pressio_search_metrics.cc

    src/search/binary.cc
    src/search/fraz.cc
    src/search/guess.cc

    src/search_metrics/progress_printer.cc
  #public headers

  #private headers
  )
target_include_directories(
  libpressio_opt
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libpressio_opt>
  )
target_link_libraries(libpressio_opt PUBLIC LibDistributed::libdistributed LibPressio::libpressio)
target_compile_features(libpressio_opt PUBLIC cxx_std_${LIBPRESSIO_OPT_CXX_VERSION})

option(USE_CLANG_TIDY "include clang-tidy warnings in the build log" OFF)
if(USE_CLANG_TIDY)
  find_program(CLANG_TIDY clang-tidy)
  set_target_properties(libpressio_opt PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY}")
  set_target_properties(libpressio_opt PROPERTIES C_CLANG_TIDY "${CLANG_TIDY}")
endif()

export(TARGETS libpressio_opt NAMESPACE LibPressioOpt:: FILE
  LibPressioOptConfig.cmake)
install(TARGETS libpressio_opt EXPORT LibPressioOptConfig
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
install(EXPORT LibPressioOptConfig NAMESPACE LibPressioOpt:: DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/LibPressioOpt/cmake)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libpressio_opt)

option(BUILD_DOCS "build the documetation" OFF)
if(BUILD_DOCS)
  find_package(Doxygen REQUIRED dot)
  set(DOXYGEN_GENERATE_HTML YES)
  set(DOXYGEN_MAN_LINKS YES)
  set(DOXYGEN_GENERATE_MAN YES)
  set(DOXYGEN_EXTRACT_LOCAL_METHODS YES)
  set(DOXYGEN_EXTRACT_STATIC YES)
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
  doxygen_add_docs(
    docs
    ${PROJECT_SOURCE_DIR}/README.md
    ${PROJECT_SOURCE_DIR}/include
    COMMENT "Generate Documenation"
  )
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
