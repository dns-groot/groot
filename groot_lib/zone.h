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
#include "resource_record.h"
#include "graph.h"

using namespace std;


//Zone Graph
struct ZoneVertex {
	Label name;
	vector<ResourceRecord> rrs;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& name;
		ar& rrs;
	}
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, ZoneVertex> ZoneGraph;
typedef boost::graph_traits<ZoneGraph>::vertex_descriptor ZoneVertexDescriptor;
typedef boost::graph_traits<ZoneGraph>::edge_descriptor ZoneEdgeDescriptor;


struct Zone {
	ZoneGraph g;
	ZoneVertexDescriptor startVertex =0;
	vector<Label> origin;
	std::map<VertexDescriptor, LabelMap> domainChildLabelMap;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& g;
		ar& startVertex;
		ar& origin;
	}
};

//Zone parser
void ParseZoneFile(string& file, LabelGraph& g, const VertexDescriptor& root, Zone& z);
void ZoneGraphBuilder(ResourceRecord& record, Zone& z);
void ZoneGraphBuilder(vector<ResourceRecord>& rrs, Zone& z);
void BuildZoneLabelGraphs(string filePath, string nameServer, LabelGraph& g, const VertexDescriptor& root, std::map<string, std::vector<Zone>>& nameServer_Zone_Map);
boost::optional<vector<ResourceRecord>> QueryLookUp(Zone& z, EC& query, bool& completeMatch);
