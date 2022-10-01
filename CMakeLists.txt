cmake_minimum_required(VERSION 3.12)
project(libpressio_opt VERSION "0.13.4" LANGUAGES C CXX)

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


include(GNUInstallDirs)

find_package(LibPressio REQUIRED)
find_package(MPI REQUIRED COMPONENTS CXX)
find_package(LibDistributed REQUIRED)
find_package(dlib REQUIRED)
find_package(std_compat REQUIRED)
find_package(PkgConfig)
find_package(Threads)

add_library(libpressio_opt
  #core features
    src/pressio_opt.cc
    src/pressio_search.cc
    src/pressio_search_metrics.cc

    src/search/binary.cc
    src/search/fraz.cc
    src/search/guess.cc
    src/search/dist_grid.cc
    src/search/nrt.cc
    src/search/guess_first.cc
    src/search/guess_midpoint.cc
    src/search/random.cc
    src/search/sdr_search.cc

    src/search_metrics/noop.cc
    src/search_metrics/progress_printer.cc
    src/search_metrics/record_search.cc
    src/search_metrics/composite_search.cc
  #public headers

  #private headers
  )
target_include_directories(
  libpressio_opt
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libpressio_opt>
  )
target_include_directories(
  libpressio_opt
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )
target_link_libraries(libpressio_opt PUBLIC LibDistributed::libdistributed std_compat::std_compat
  LibPressio::libpressio PRIVATE dlib::dlib )
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libpressio_opt_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/libpressio_opt_version.h
  )
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/libpressio_opt.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libpressio_opt.pc
  @ONLY
  )

option(USE_CLANG_TIDY "include clang-tidy warnings in the build log" OFF)
if(USE_CLANG_TIDY)
  find_program(CLANG_TIDY clang-tidy)
  set_target_properties(libpressio_opt PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY}")
  set_target_properties(libpressio_opt PROPERTIES C_CLANG_TIDY "${CLANG_TIDY}")
endif()

install(TARGETS libpressio_opt EXPORT LibPressioOptTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
install(EXPORT LibPressioOptTargets NAMESPACE LibPressioOpt:: DESTINATION
  ${CMAKE_INSTALL_LIBDIR}/cmake/LibPressioOpt/)
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/LibPressioOptConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/LibPressioOptConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibPressioOpt
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/LibPressioOptConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LibPressioOptConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LibPressioOptConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LibPressioOpt
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libpressio_opt)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libpressio_opt.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pkgconfig)

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
    ${PROJECT_SOURCE_DIR}/docs
    ${PROJECT_SOURCE_DIR}/README.md
    ${PROJECT_SOURCE_DIR}/include
    COMMENT "Generate Documenation"
  )
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

