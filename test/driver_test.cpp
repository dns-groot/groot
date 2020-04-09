#define BOOST_TEST_MODULE DRIVER_TEST_

#include <boost/test/unit_test.hpp>
#include "../src/driver.h"


class DriverTest {

public:
	int GetNumberofLabelGraphVertices(Driver& d) {
		return num_vertices(d.label_graph_);
	}

	int GetNumberofLabelGraphEdges(Driver& d) {
		return num_edges(d.label_graph_);
	}

	int GetNumberofResourceRecordsParsed(Driver& d, string file, string nameserver) {
		return d.ParseZoneFileAndExtendGraphs(file, nameserver);
	}
};

BOOST_AUTO_TEST_SUITE(ParsingTestSuite)

BOOST_AUTO_TEST_CASE(my_boost_test)
{
	std::string test_file = "..//test//ExampleZones//test_parser.txt";
	Driver d;
	DriverTest dt;

	BOOST_TEST(7 == dt.GetNumberofResourceRecordsParsed(d, test_file, "ns1.test."));
	BOOST_TEST(5 == dt.GetNumberofLabelGraphVertices(d));
	BOOST_TEST(4 == dt.GetNumberofLabelGraphEdges(d));
}

BOOST_AUTO_TEST_SUITE_END()