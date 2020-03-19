cmake_minimum_required(VERSION 3.10.2)
set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(libgroot CXX)

find_package(Boost REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(CMAKE_BUILD_TYPE Release)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES "*.cpp")

add_library(libgroot STATIC ${SOURCES})

target_link_libraries(libgroot PRIVATE nlohmann_json spdlog::spdlog)

set_target_properties(libgroot PROPERTIES PREFIX "")
