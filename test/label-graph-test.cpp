#include "driver-test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(ParsingTestSuite)

void CheckClosestEncloser(string d1, string d2, string d3, string query, string expectedLabel)
{
    label::Graph labelGraph;
    zone::Graph zoneGraph(0);

    for (int i = 0; i < RRType::N; i++) {
        string type = TypeUtils::TypeToString(static_cast<RRType>(i));

        ResourceRecord r1(d1, type, 1, 10, "");
        auto id1 = zoneGraph.AddResourceRecord(r1);
        labelGraph.AddResourceRecord(r1, 0, id1.get());

        ResourceRecord r2(d2, type, 1, 10, "");
        auto id2 = zoneGraph.AddResourceRecord(r2);
        if (id2)
            labelGraph.AddResourceRecord(r2, 0, id2.get());

        ResourceRecord r3(d3, type, 1, 10, "");
        auto id3 = zoneGraph.AddResourceRecord(r3);
        if (id3)
            labelGraph.AddResourceRecord(r3, 0, id3.get());

        auto enclosers = labelGraph.ClosestEnclosers(query);
        auto x = labelGraph[enclosers[0].first];
        BOOST_CHECK_EQUAL(expectedLabel, x.name.get());
    }
}

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

BOOST_AUTO_TEST_CASE(label_graph_dnames)
{
    label::Graph labelGraph;
    zone::Graph zoneGraph(0);

    ResourceRecord r1("foo.com", "A", 1, 10, "1.2.3.4");
    auto id1 = zoneGraph.AddResourceRecord(r1);
    labelGraph.AddResourceRecord(r1, 0, id1.get());

    ResourceRecord r2("bar.com", "DNAME", 1, 10, "foo.com");
    auto id2 = zoneGraph.AddResourceRecord(r2);
    labelGraph.AddResourceRecord(r2, 0, id2.get());

    ResourceRecord r3("foo.com", "DNAME", 1, 10, "bar.com");
    auto id3 = zoneGraph.AddResourceRecord(r3);
    labelGraph.AddResourceRecord(r3, 0, id3.get());

    auto enclosers = labelGraph.ClosestEnclosers("a.foo.com");
    auto x = labelGraph[enclosers[0].first];
    BOOST_CHECK_EQUAL("foo", x.name.get());
    BOOST_CHECK_EQUAL(2, x.rrtypes_available.count());
}

BOOST_AUTO_TEST_CASE(label_graph_examples)
{
    CheckClosestEncloser("com", "a.com", "b.a.com", "c.b.a.com", "b");
    CheckClosestEncloser("org", "com", "net", "a.b.c.org", "org");
    CheckClosestEncloser("a.org", "aa.org", "aaa.org", "aaa.org", "aaa");
    CheckClosestEncloser("a.org", "aa.org", "aaa.org", "aaaa.org", "org");
    CheckClosestEncloser("com", "com", "com", "com", "com");
}

BOOST_AUTO_TEST_CASE(label_graph_many_children)
{
    label::Graph labelGraph;
    zone::Graph zoneGraph(0);

    int max = kHashMapThreshold + 50;

    // exceed the hashmap threshold for the TLD
    for (int i = 0; i <= max; i++) {
        string domain = "d" + std::to_string(i);
        ResourceRecord r(domain, "TXT", 1, 10, domain);
        auto id = zoneGraph.AddResourceRecord(r);
        labelGraph.AddResourceRecord(r, 0, id.get());
    }

    // exceed the threshold for a second level domain
    for (int i = 0; i <= max; i++) {
        string domain = "d" + std::to_string(i) + ".d0";
        ResourceRecord r(domain, "TXT", 1, 10, domain);
        auto id = zoneGraph.AddResourceRecord(r);
        labelGraph.AddResourceRecord(r, 0, id.get());
    }

    for (int i = 0; i <= max; i++) {
        string domain = "d" + std::to_string(i);
        auto enclosers = labelGraph.ClosestEnclosers(domain);
        auto x = labelGraph[enclosers[0].first];
        BOOST_CHECK_EQUAL(domain, x.name.get());
        BOOST_CHECK_EQUAL(1, x.rrtypes_available.count());
    }

    for (int i = 0; i <= max; i++) {
        string domain = "d" + std::to_string(i) + ".d0";
        auto enclosers = labelGraph.ClosestEnclosers(domain);
        auto x = labelGraph[enclosers[0].first];
        BOOST_CHECK_EQUAL("d" + std::to_string(i), x.name.get());
        BOOST_CHECK_EQUAL(1, x.rrtypes_available.count());
    }
}

BOOST_AUTO_TEST_SUITE_END()