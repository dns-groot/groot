#include "driver-test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(ParsingTestSuite)

BOOST_AUTO_TEST_CASE(label_graph_building)
{
    Driver d;
    DriverTest dt;
    boost::filesystem::path directory("TestFiles");
    std::string test_file = (directory / "parser_tests" / "test_parser.txt").string();
    BOOST_TEST(8 == dt.GetNumberofResourceRecordsParsed(d, test_file, "ns1.test."));
    BOOST_TEST(7 == dt.GetNumberofLabelGraphVertices(d));
    BOOST_TEST(7 == dt.GetNumberofLabelGraphEdges(d));
}

BOOST_AUTO_TEST_SUITE_END()