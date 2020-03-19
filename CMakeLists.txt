cmake_minimum_required(VERSION 3.10.2)

# Refuse to build outside `build`
if (NOT ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}/build"))
	message(FATAL_ERROR "Please run `mkdir ${CMAKE_SOURCE_DIR}/build` and run `cmake ..` within it.")
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(groot CXX)

add_subdirectory(src)
add_subdirectory(bin)
