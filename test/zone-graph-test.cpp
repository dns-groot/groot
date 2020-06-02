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

BOOST_AUTO_TEST_CASE(multiple_CNAME)
{
    zone::Graph zoneGraph(0);
    ResourceRecord r1("foo.com", "CNAME", 1, 10, "bar.com");
    auto [code1, id1] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code1 == zone::RRAddCode::SUCCESS);

    auto [code2, id2] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code2 == zone::RRAddCode::DUPLICATE); // Duplicate record

    ResourceRecord r3("foo.com", "CNAME", 1, 10, "aaa.com");
    auto [code3, id3] = zoneGraph.AddResourceRecord(r3);
    BOOST_CHECK(code3 == zone::RRAddCode::CNAME_MULTIPLE); // Multiple CNAME records

    ResourceRecord r4("foo.com", "A", 1, 10, "1.2.3.4");
    auto [code4, id4] = zoneGraph.AddResourceRecord(r4);
    BOOST_CHECK(code4 == zone::RRAddCode::CNAME_OTHER); // Other records at CNAME node should not return SUCCESS
}

BOOST_AUTO_TEST_CASE(multiple_DNAME)
{
    zone::Graph zoneGraph(0);
    ResourceRecord r1("foo.com", "DNAME", 1, 10, "bar.com");
    auto [code1, id1] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code1 == zone::RRAddCode::SUCCESS);

    auto [code2, id2] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code2 == zone::RRAddCode::DUPLICATE); // Duplicate record

    ResourceRecord r3("foo.com", "DNAME", 1, 10, "aaa.com");
    auto [code3, id3] = zoneGraph.AddResourceRecord(r3);
    BOOST_CHECK(code3 == zone::RRAddCode::DNAME_MULTIPLE); // Multiple DNAME records

    ResourceRecord r4("foo.com", "A", 1, 10, "1.2.3.4");
    auto [code4, id4] = zoneGraph.AddResourceRecord(r4);
    BOOST_CHECK(code4 == zone::RRAddCode::SUCCESS); // Other records at DNAME node should return success
}

BOOST_AUTO_TEST_SUITE_END()