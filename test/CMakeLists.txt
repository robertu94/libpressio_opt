configure_file(GTestCMakeLists.txt.in googletest-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download
)
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download
)
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
  EXCLUDE_FROM_ALL)
include(GoogleTest)
find_package(MPI)

function(add_mpi_gtest)
  get_filename_component(test_name ${ARGV0} NAME_WE)
  add_executable(${test_name} ${ARGV} mpi_test_main.cc)
  target_link_libraries(${test_name} PUBLIC libdistributed gtest gmock)
  add_test(NAME ${test_name} COMMAND ${MPIEXEC_EXECUTABLE}
    "${MPIEXEC_NUMPROC_FLAG}" "${MPIEXEC_MAX_NUMPROCS}" "${CMAKE_CURRENT_BINARY_DIR}/${test_name}")
endfunction()

find_package(PkgConfig)
find_package(SZ REQUIRED)
find_package(ZLIB REQUIRED)
pkg_search_module(ZSTD  IMPORTED_TARGET libzstd)
pkg_search_module(FFTW3 IMPORTED_TARGET fftw3)

add_executable(opt_example opt_example.cc)
target_link_libraries(opt_example PUBLIC LibPressio::libpressio libpressio_opt SZ)
add_test(opt_example opt_example)

add_executable(opt_example_c opt_example_c.c)
target_link_libraries(opt_example_c PUBLIC LibPressio::libpressio libpressio_opt SZ)
