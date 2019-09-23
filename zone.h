#pragma once
#include <string>
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <iostream>
#include "RR.h"
#include "graph.h"

using namespace std;

//Zone parser
vector<ResourceRecord> parse_zone_file(string& file);

//Zone Graph
struct zoneVertex {
	string name;
	vector<ResourceRecord> rrs;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& name;
		ar& rrs;
	}
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, zoneVertex> zoneGraph;
typedef boost::graph_traits<zoneGraph>::vertex_descriptor zoneVertex_t;
typedef boost::graph_traits<zoneGraph>::edge_descriptor zoneEdge_t;


struct zone {
	zoneGraph g;
	zoneVertex_t startVertex;
	vector<std::string> origin;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& g;
		ar& startVertex;
		ar& origin;
	}
};


void zone_graph_builder(vector<ResourceRecord>& rrs, zone& z);
boost::optional<vector<ResourceRecord>> queryLookUp(zone& z, EC& query, bool& completeMatch);