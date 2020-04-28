cmake_minimum_required(VERSION 3.10.2)

# Refuse to build outside `build`
if (NOT ("${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/build"))
	message(FATAL_ERROR "Please run `mkdir ${CMAKE_CURRENT_SOURCE_DIR}/build` and run `cmake ..` within it.")
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(groot CXX)

# Code Coverage Configuration
add_library(coverage_config INTERFACE)

option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Add required flags (GCC & LLVM/Clang)
  target_compile_options(coverage_config INTERFACE
    -O0        # no optimization
    -g         # generate debug info
    --coverage # sets all required flags
  )
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
    target_link_options(coverage_config INTERFACE --coverage)
  else()
    target_link_libraries(coverage_config INTERFACE --coverage)
  endif()
endif()

add_subdirectory(src)

option (BUILD_TESTING "Build the testing tree." OFF)
if (BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
else()
  add_subdirectory(bin)
endif()