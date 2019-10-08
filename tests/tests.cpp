#define BOOST_TEST_MODULE mytests
#include <boost/test/unit_test.hpp>
#include <cassert>
#include <iomanip>
#include <vector>
#include <iostream>
#include "../groot_lib/resource_record.h"
#include "../groot_lib/graph.h"
#include "../groot_lib/zone.h"
#include "../groot_lib/interpreter.h"
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

BOOST_AUTO_TEST_SUITE(ExampleTestSuite)

BOOST_AUTO_TEST_CASE(my_boost_test)
{
	std::string directory = "..\\demo\\zone_files\\";
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set(".");

	std::ifstream metadataFile((boost::filesystem::path{ directory } / boost::filesystem::path{ "metadata.json" }).string());
	json metadata;
	metadataFile >> metadata;
	for (auto& server : metadata["TopNameServers"]) {
		gTopNameServers.push_back(server);
	}
	for (auto& zone : metadata["ZoneFiles"]) {
		std::string f = zone["FileName"];
		BuildZoneLabelGraphs((boost::filesystem::path{ directory } / boost::filesystem::path{ f }).string(), zone["NameServer"], g, root, gNameServerZoneMap);
	}
	
	BOOST_TEST(num_edges(g) == 41);
	BOOST_TEST(num_vertices(g) == 41);
}

//BOOST_AUTO_TEST_CASE(small_zone_test)
//{
//	std::string directory = "..\\tests\\ExampleZones\\";
//	LabelGraph g;
//	VertexDescriptor root = boost::add_vertex(g);
//	g[root].name.set(".");
//	BuildZoneLabelGraphs(directory + "child.mtn.net.sy (2).txt", "ns2.child.mtn.net.sy.", g, root, gNameServerZoneMap);
//	vector<EC> allQueries;
//	ECGenerator(g, root, allQueries);
//	int count = 0;
//	for (auto ec : allQueries) {
//		if (ec.name == GetLabels("ns1.child.mtn.net.sy.")) {
//			count++;
//		}
//	}
//	BOOST_TEST(num_edges(g) == 6);
//	BOOST_TEST(num_vertices(g) == 7);
//	BOOST_TEST(allQueries.size() == 17);
//	BOOST_TEST(count == 3);
//}
//
//BOOST_AUTO_TEST_CASE(cname_zone_test)
//{
//	std::string directory = "..\\tests\\ExampleZones\\";
//	LabelGraph g;
//	VertexDescriptor root = boost::add_vertex(g);
//	g[root].name.set(".");
//	BuildZoneLabelGraphs(directory + "mtn.net.sy.txt", "ns1.mtn.net.sy.", g, root, gNameServerZoneMap);
//	vector<EC> allQueries;
//	ECGenerator(g, root, allQueries);
//	int count = 0;
//	for (auto ec : allQueries) {
//		if (ec.name == GetLabels("ns1.child.mtn.net.sy.")) {
//			count++;
//		}
//		if (ec.name == GetLabels("bar.mtn.net.sy.")) {
//			if (ec.rrTypes[RRType::CNAME] == 1) {
//				BOOST_TEST(ec.rrTypes[RRType::A] == 1);
//			}
//		}
//	}
//	BOOST_TEST(num_edges(g) == 12);
//	BOOST_TEST(num_vertices(g) == 12);
//	BOOST_TEST(allQueries.size() == 33);
//	BOOST_TEST(count == 3);
//}
//
BOOST_AUTO_TEST_CASE(lexerTest)
{
	std::string directory = "..\\tests\\ExampleZones\\";
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set(".");
	BuildZoneLabelGraphs(directory + "test_lexer.txt", "ns1.net.", g, root, gNameServerZoneMap);
	BOOST_TEST(num_edges(g) == 12);
	BOOST_TEST(num_vertices(g) == 12);
}

BOOST_AUTO_TEST_SUITE_END()
