#include <boost/test/unit_test.hpp>
#include "driver-test.h"


BOOST_AUTO_TEST_SUITE(ZoneGraphTestSuite)

BOOST_AUTO_TEST_CASE(zone_graph_building)
{
	std::string test_file = "..//test//ExampleZones//test_parser.txt";
	Driver d;
	DriverTest dt;
	BOOST_TEST(8 == dt.GetNumberofResourceRecordsParsed(d, test_file, "ns1.test."));
	const zone::Graph& g = dt.GetLatestZone(d);
	BOOST_TEST(6 == num_vertices(g));
	BOOST_TEST(5 == num_edges(g));
}

BOOST_AUTO_TEST_SUITE_END()