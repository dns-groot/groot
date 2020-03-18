cmake_minimum_required(VERSION 3.5.1)
set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(groot_lib CXX)

# list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../spdlog/cmake/")

# add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../spdlog/" "${CMAKE_CURRENT_BINARY_DIR}/spdlog")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(CMAKE_BUILD_TYPE Release)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB HEADER_FILES "*.h")
file(GLOB SOURCES "*.cpp")

add_library(groot STATIC ${SOURCES} ${HEADER_FILES})

target_link_libraries(groot PRIVATE nlohmann_json spdlog::spdlog)
