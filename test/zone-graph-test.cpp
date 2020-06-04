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

BOOST_AUTO_TEST_CASE(DNAME_SOA_NS)
{
    zone::Graph zoneGraph(0);
    ResourceRecord r1(
        "foo.com", "SOA", 1, 10, "us.illinois.net. us-domain.illinois.net. 2018083000 14400 3600 2419200 14400");
    auto [code1, id1] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code1 == zone::RRAddCode::SUCCESS);

    ResourceRecord r2("foo.com", "NS", 1, 10, "ns1.bar.com");
    auto [code2, id2] = zoneGraph.AddResourceRecord(r2);
    BOOST_CHECK(code2 == zone::RRAddCode::SUCCESS);

    EC longer;
    longer.name = LabelUtils::StringToLabels("alpha.foo.com");
    longer.rrTypes.set(RRType::A);

    EC exact;
    exact.name = LabelUtils::StringToLabels("foo.com");
    exact.rrTypes.set(RRType::A);

    EC negation;
    negation.name = LabelUtils::StringToLabels("foo.com");
    negation.rrTypes.set(RRType::A);
    negation.excluded = boost::make_optional(std::vector<NodeLabel>{});

    bool complete = false;

    auto longer_answer = zoneGraph.QueryLookUpAtZone(longer, complete);
    BOOST_ASSERT(longer_answer.is_initialized());
    BOOST_CHECK(std::get<0>(longer_answer.get()[0]) == ReturnTag::NX);

    auto exact_answer = zoneGraph.QueryLookUpAtZone(exact, complete);
    BOOST_ASSERT(exact_answer.is_initialized());
    BOOST_CHECK(std::get<0>(exact_answer.get()[0]) == ReturnTag::ANS);

    auto negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::NX);

    ResourceRecord r3("foo.com", "DNAME", 1, 10, "bar.com"); // DNAME is allowed to stay with NS as there is SOA
    auto [code3, id3] = zoneGraph.AddResourceRecord(r3);
    BOOST_CHECK(code3 == zone::RRAddCode::SUCCESS);

    longer_answer = zoneGraph.QueryLookUpAtZone(longer, complete);
    BOOST_ASSERT(longer_answer.is_initialized());
    BOOST_CHECK(std::get<0>(longer_answer.get()[0]) == ReturnTag::REWRITE);

    exact_answer = zoneGraph.QueryLookUpAtZone(exact, complete);
    BOOST_ASSERT(exact_answer.is_initialized());
    BOOST_CHECK(std::get<0>(exact_answer.get()[0]) == ReturnTag::ANS);

    negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::REWRITE);

    ResourceRecord r4("alpha.foo.com", "A", 1, 10, "1.1.1.1");
    auto [code4, id4] = zoneGraph.AddResourceRecord(r4);
    BOOST_CHECK(code4 == zone::RRAddCode::SUCCESS);

    // Even though there is an exact match that should be ignored because of DNAME
    longer_answer = zoneGraph.QueryLookUpAtZone(longer, complete);
    BOOST_ASSERT(longer_answer.is_initialized());
    BOOST_CHECK(std::get<0>(longer_answer.get()[0]) == ReturnTag::REWRITE);

    ResourceRecord r5("*.foo.com", "A", 1, 10, "2.2.2.2");
    auto [code5, id5] = zoneGraph.AddResourceRecord(r5);
    BOOST_CHECK(code5 == zone::RRAddCode::SUCCESS);

    // Even though there is a wildcard match that should be ignored because of DNAME
    negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::REWRITE);
}

BOOST_AUTO_TEST_CASE(DNAME_NS)
{
    zone::Graph zoneGraph(0);
    ResourceRecord r1(
        "foo.com", "SOA", 1, 10, "us.illinois.net. us-domain.illinois.net. 2018083000 14400 3600 2419200 14400");
    auto [code1, id1] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code1 == zone::RRAddCode::SUCCESS);

    ResourceRecord r2("alpha.foo.com", "NS", 1, 10, "ns1.bar.com");
    auto [code2, id2] = zoneGraph.AddResourceRecord(r2);
    BOOST_CHECK(code2 == zone::RRAddCode::SUCCESS);

    EC longer;
    longer.name = LabelUtils::StringToLabels("beta.alpha.foo.com");
    longer.rrTypes.set(RRType::A);

    EC exact;
    exact.name = LabelUtils::StringToLabels("alpha.foo.com");
    exact.rrTypes.set(RRType::A);

    EC negation;
    negation.name = LabelUtils::StringToLabels("alpha.foo.com");
    negation.rrTypes.set(RRType::A);
    negation.excluded = boost::make_optional(std::vector<NodeLabel>{});

    bool complete = false;

    auto longer_answer = zoneGraph.QueryLookUpAtZone(longer, complete);
    BOOST_ASSERT(longer_answer.is_initialized());
    BOOST_CHECK(std::get<0>(longer_answer.get()[0]) == ReturnTag::REF);

    auto exact_answer = zoneGraph.QueryLookUpAtZone(exact, complete);
    BOOST_ASSERT(exact_answer.is_initialized());
    BOOST_CHECK(std::get<0>(exact_answer.get()[0]) == ReturnTag::REF);

    auto negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::REF);

    ResourceRecord r3("alpha.foo.com", "DNAME", 1, 10, "bar.com"); // By RFC6672 DNAME and NS can not stay together.
    auto [code3, id3] = zoneGraph.AddResourceRecord(r3);
    BOOST_CHECK(code3 == zone::RRAddCode::SUCCESS);

    // DNAME should be ignored by all the ECs as there is no SOA and NS takes priority (zone-cut)
    longer_answer = zoneGraph.QueryLookUpAtZone(longer, complete);
    BOOST_ASSERT(longer_answer.is_initialized());
    BOOST_CHECK(std::get<0>(longer_answer.get()[0]) == ReturnTag::REF);

    exact_answer = zoneGraph.QueryLookUpAtZone(exact, complete);
    BOOST_ASSERT(exact_answer.is_initialized());
    BOOST_CHECK(std::get<0>(exact_answer.get()[0]) == ReturnTag::REF);

    negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::REF);

    ResourceRecord r5("*.alpha.foo.com", "A", 1, 10, "2.2.2.2");
    auto [code5, id5] = zoneGraph.AddResourceRecord(r5);
    BOOST_CHECK(code5 == zone::RRAddCode::SUCCESS);

    // Even though there is a wildcard match that should be ignored because of zone-cut
    negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::REF);
}

BOOST_AUTO_TEST_CASE(Wildcard)
{
    zone::Graph zoneGraph(0);
    ResourceRecord r1(
        "foo.com", "SOA", 1, 10, "us.illinois.net. us-domain.illinois.net. 2018083000 14400 3600 2419200 14400");
    auto [code1, id1] = zoneGraph.AddResourceRecord(r1);
    BOOST_CHECK(code1 == zone::RRAddCode::SUCCESS);

    ResourceRecord r2("*.foo.com", "A", 1, 10, "1.1.1.1");
    auto [code2, id2] = zoneGraph.AddResourceRecord(r2);
    BOOST_CHECK(code2 == zone::RRAddCode::SUCCESS);

    EC longer;
    longer.name = LabelUtils::StringToLabels("beta.alpha.foo.com");
    longer.rrTypes.set(RRType::A);
    longer.rrTypes.set(RRType::MX);

    EC negation;
    negation.name = LabelUtils::StringToLabels("foo.com");
    negation.rrTypes.set(RRType::A);
    negation.rrTypes.set(RRType::MX);
    negation.excluded = boost::make_optional(std::vector<NodeLabel>{});

    bool complete = false;

    auto longer_answer = zoneGraph.QueryLookUpAtZone(longer, complete);
    BOOST_ASSERT(longer_answer.is_initialized());
    BOOST_CHECK(std::get<0>(longer_answer.get()[0]) == ReturnTag::ANS); // for A
    BOOST_CHECK(std::get<0>(longer_answer.get()[1]) == ReturnTag::ANS); // for MX

    auto negation_answer = zoneGraph.QueryLookUpAtZone(negation, complete);
    BOOST_ASSERT(negation_answer.is_initialized());
    BOOST_CHECK(std::get<0>(negation_answer.get()[0]) == ReturnTag::ANS);
    BOOST_CHECK(std::get<0>(negation_answer.get()[1]) == ReturnTag::ANS);

    ResourceRecord r3("*.*.foo.com", "CNAME", 1, 10, "aaa.com"); // wildcard under a wildcard
    auto [code3, id3] = zoneGraph.AddResourceRecord(r3);
    BOOST_CHECK(code3 == zone::RRAddCode::SUCCESS);

    EC star;
    star.name = LabelUtils::StringToLabels("alpha.*.foo.com");
    star.rrTypes.set(RRType::A);
    star.rrTypes.set(RRType::MX);

    auto star_answer = zoneGraph.QueryLookUpAtZone(star, complete);
    BOOST_ASSERT(star_answer.is_initialized());
    BOOST_CHECK(std::get<0>(star_answer.get()[0]) == ReturnTag::REWRITE);

    star.rrTypes.reset(RRType::A);
    star.rrTypes.reset(RRType::MX);
    star.rrTypes.set(RRType::CNAME);

    star_answer = zoneGraph.QueryLookUpAtZone(star, complete);
    BOOST_ASSERT(star_answer.is_initialized());
    BOOST_CHECK(std::get<0>(star_answer.get()[0]) == ReturnTag::ANS); // If CNAME is the only type requested.

    EC double_start;
    double_start.name = LabelUtils::StringToLabels("alpha.*.*.foo.com");
    double_start.rrTypes.set(RRType::A);

    auto double_star_answer = zoneGraph.QueryLookUpAtZone(double_start, complete);
    BOOST_ASSERT(double_star_answer.is_initialized());
    BOOST_CHECK(std::get<0>(double_star_answer.get()[0]) == ReturnTag::NX);
}

BOOST_AUTO_TEST_SUITE_END()