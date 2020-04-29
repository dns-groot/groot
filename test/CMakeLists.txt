cmake_minimum_required(VERSION 3.10.2)

if(NOT TARGET libgroot)
	message(FATAL_ERROR "Please run CMake at the repository root and then try `make groot`")
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(groot_test CXX)

find_package(Boost COMPONENTS regex filesystem unit_test_framework REQUIRED)
find_package(docopt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set ( PROJECT_LINK_LIBS libgroot.a )
#link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../build/")


include_directories(${Boost_INCLUDE_DIRS})

file(GLOB SRCS "*.cpp")
add_executable(tester ${SRCS})

target_link_libraries(tester PRIVATE libgroot ${Boost_LIBRARIES} 
	                            docopt_s nlohmann_json spdlog::spdlog)

foreach(testSrc ${SRCS})
	file(READ "${testSrc}" SOURCE_FILE_CONTENTS)
	string(REGEX MATCHALL "BOOST_AUTO_TEST_SUITE\\( *([A-Za-z_0-9]+) *\\)" 
           FOUND_SUITE ${SOURCE_FILE_CONTENTS})
	foreach(HIT ${FOUND_SUITE})
		string(REGEX MATCHALL "BOOST_AUTO_TEST_CASE\\( *([A-Za-z_0-9]+) *\\)" 
           FOUND_TESTS ${SOURCE_FILE_CONTENTS})
		string(REGEX REPLACE ".*\\( *([A-Za-z_0-9]+) *\\).*" "\\1" SUIT_NAME ${HIT})
		foreach(HITT ${FOUND_TESTS})
			string(REGEX REPLACE ".*\\( *([A-Za-z_0-9]+) *\\).*" "\\1" TEST_NAME ${HITT})
			add_test(NAME "tester.${SUIT_NAME}/${TEST_NAME}" COMMAND tester --run_test=${SUIT_NAME}/${TEST_NAME})
		endforeach()
	endforeach()
endforeach()


#add_test(NAME tests COMMAND tester)                     
#add_test(NAME "tester.DemoTestSuite/demo_test" COMMAND tester --run_test=DemoTestSuite/demo_test)                    
#add_test(NAME "tester.ParsingTestSuite/label_graph_building" COMMAND tester --run_test=ParsingTestSuite/label_graph_building)         
#add_test(NAME "tester.ZoneGraphTestSuite/zone_graph_building" COMMAND tester --run_test=ZoneGraphTestSuite/zone_graph_building)          