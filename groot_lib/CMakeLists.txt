cmake_minimum_required(VERSION 3.5.1)

project(groot_lib CXX)

if(NOT TARGET spdlog)
    find_package(spdlog REQUIRED)
endif()

set(CMAKE_BUILD_TYPE Release)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB HEADER_FILES "*.h")
file(GLOB SOURCES "*.cpp")

add_library(groot_lib STATIC ${SOURCES} ${HEADER_FILES})
target_link_libraries(groot_lib PRIVATE spdlog::spdlog)
