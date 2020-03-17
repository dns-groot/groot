cmake_minimum_required(VERSION 3.5.1)

project(groot CXX)

set(CMAKE_BUILD_TYPE Release)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#add_library(groot_lib STATIC ${SOURCES} ${HEADER_FILES})

add_library(spdlog STATIC )
