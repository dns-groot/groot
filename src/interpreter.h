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
#include <map> 
#include "resource_record.h"
#include "graph.h"
#include "zone.h"

struct InterpreterVertex {
	std::string ns;
	EC query;
	boost::optional<vector<ResourceRecord>> answer;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& ns;
		ar& query;
		ar& answer;
	}
};

struct InterpreterEdge {
	boost::optional<int> intermediateQuery;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& intermediateQuery;
	}
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, InterpreterVertex, InterpreterEdge> InterpreterGraph;

//Some typedefs for simplicity
typedef boost::graph_traits<InterpreterGraph>::vertex_descriptor InterpreterVertexDescriptor;
typedef boost::graph_traits<InterpreterGraph>::edge_descriptor InterpreterEdgeDescriptor;

struct InterpreterGraphWrapper {
	InterpreterGraph intG;
	InterpreterVertexDescriptor startVertex = 0;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& intG;
		ar& startVertex;
	}
};
typedef vector<InterpreterVertexDescriptor> Path;
extern std::map<string, std::vector<Zone>> gNameServerZoneMap;
extern std::vector<string> gTopNameServers;

vector<ResourceRecord> SeparateGlueRecords(vector<ResourceRecord> records);
void GenerateDotFileInterpreter(string outputfile, InterpreterGraph& g);
bool CheckQueryEquivalence(EC& query, EC& nodeQuery);
boost::optional<InterpreterVertexDescriptor> InsertNode(std::map<string, std::vector<InterpreterVertexDescriptor>>& nameServer_nodes_map, InterpreterGraph& intG, string ns, EC query, InterpreterVertexDescriptor edgeStart, boost::optional<InterpreterVertexDescriptor> edgeQuery);
void StartFromTop(InterpreterGraph& intG, InterpreterVertexDescriptor edgeStartNode, EC& query, std::map<string, std::vector<InterpreterVertexDescriptor>>& nameServer_nodes_map);

void BuildInterpretationGraph(EC& query, InterpreterGraphWrapper& intGraph_wrapper);
