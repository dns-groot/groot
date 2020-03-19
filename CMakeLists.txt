cmake_minimum_required(VERSION 3.10.2)
set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(groot CXX)

add_subdirectory(src)
add_subdirectory(bin)
