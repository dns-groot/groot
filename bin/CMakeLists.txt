cmake_minimum_required(VERSION 3.10.2)

if(NOT TARGET libgroot)
	message(FATAL_ERROR "Please run CMake at the repository root and then try `make groot`")
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(groot_bin CXX)

find_package(Boost COMPONENTS regex REQUIRED)
find_package(docopt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set ( PROJECT_LINK_LIBS libgroot.a )
#link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../build/")

include_directories(${Boost_INCLUDE_DIRS})
add_executable(groot groot.cpp)

target_link_libraries(groot PRIVATE stdc++fs ${Boost_LIBRARIES}
	                            docopt_s nlohmann_json spdlog::spdlog
                                    libgroot)
