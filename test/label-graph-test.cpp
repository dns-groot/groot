#include "driver-test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(ParsingTestSuite)

BOOST_AUTO_TEST_CASE(label_graph_building)
{
    Driver d;
    DriverTest dt;
    boost::filesystem::path directory("TestFiles");
    std::string test_file = (directory / "parser_tests" / "test_parser.txt").string();
    BOOST_CHECK_EQUAL(8, dt.GetNumberofResourceRecordsParsed(d, test_file, "ns1.test."));
    BOOST_CHECK_EQUAL(7, dt.GetNumberofLabelGraphVertices(d));
    BOOST_CHECK_EQUAL(7, dt.GetNumberofLabelGraphEdges(d));
}


BOOST_AUTO_TEST_CASE(label_graph_search)
{
    label::Graph labelGraph;
    zone::Graph zoneGraph(0);

    ResourceRecord r1("foo.com.", "A", 1, 10, "1.2.3.4");
    auto id1 = zoneGraph.AddResourceRecord(r1);
    labelGraph.AddResourceRecord(r1, 0, id1.get());

    auto enclosers = labelGraph.ClosestEnclosers("foo.com.");
    auto node = labelGraph[enclosers[0].first];
    BOOST_CHECK_EQUAL(1, node.rrtypes_available.count());

    ResourceRecord r2("foo.com", "NS", 1, 10, "ns1.foo.com");
    auto id2 = zoneGraph.AddResourceRecord(r2);
    labelGraph.AddResourceRecord(r2, 0, id2.get());

    enclosers = labelGraph.ClosestEnclosers("foo.com");
    node = labelGraph[enclosers[0].first];
    BOOST_CHECK_EQUAL(2, node.rrtypes_available.count());
}

BOOST_AUTO_TEST_SUITE_END()