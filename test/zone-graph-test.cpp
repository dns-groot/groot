#include "driver-test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(ZoneGraphTestSuite)

BOOST_AUTO_TEST_CASE(zone_graph_building)
{
    Driver d;
    DriverTest dt;
    boost::filesystem::path directory("TestFiles");
    std::string test_file = (directory / "parser_tests" / "test_parser.txt").string();
    cout << test_file << endl;
    BOOST_TEST(8 == dt.GetNumberofResourceRecordsParsed(d, test_file, "ns1.test."));
    const zone::Graph &g = dt.GetLatestZone(d);
    BOOST_TEST(6 == num_vertices(g));
    BOOST_TEST(5 == num_edges(g));
}

BOOST_AUTO_TEST_SUITE_END()