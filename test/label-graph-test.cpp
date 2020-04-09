#define BOOST_TEST_MODULE DRIVER_TEST_

#include <boost/test/unit_test.hpp>
#include "driver-test.h"


BOOST_AUTO_TEST_SUITE(ParsingTestSuite)

BOOST_AUTO_TEST_CASE(label_graph_building)
{
	std::string test_file = "..//test//ExampleZones//test_parser.txt";
	Driver d;
	DriverTest dt;

	BOOST_TEST(8 == dt.GetNumberofResourceRecordsParsed(d, test_file, "ns1.test."));
	BOOST_TEST(7 == dt.GetNumberofLabelGraphVertices(d));
	BOOST_TEST(7 == dt.GetNumberofLabelGraphEdges(d));
}

BOOST_AUTO_TEST_SUITE_END()