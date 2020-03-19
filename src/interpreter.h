#pragma once

#include <iostream>
#include <map> 
#include <string>
#include <vector>

#include <boost/graph/graphviz.hpp>

#include "graph.h"
#include "resource_record.h"
#include "zone.h"


struct InterpreterVertex {
	std::string ns;
	EC query;
	boost::optional<vector<ZoneLookUpAnswer>> answer;
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

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, InterpreterVertex, InterpreterEdge> InterpreterGraph;

//Some typedefs for simplicity
typedef boost::graph_traits<InterpreterGraph>::vertex_descriptor IntpVD;
typedef boost::graph_traits<InterpreterGraph>::edge_descriptor IntpED;

typedef std::map<string, std::vector<IntpVD>> NameServerIntpre;

struct InterpreterGraphWrapper {
	InterpreterGraph intG;
	IntpVD startVertex = 0;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& intG;
		ar& startVertex;
	}
};
typedef vector<IntpVD> Path;


vector<ResourceRecord> SeparateGlueRecords(vector<ResourceRecord> records);
void GenerateDotFileInterpreter(string outputfile, InterpreterGraph& g);
bool CheckQueryEquivalence(EC& query, EC& nodeQuery);
boost::optional<IntpVD> InsertNode(NameServerIntpre& nameServer_nodes_map, InterpreterGraph& intG, string ns, EC query, IntpVD edgeStart, boost::optional<IntpVD> edgeQuery);
void StartFromTop(InterpreterGraph& intG, IntpVD edgeStartNode, EC& query, NameServerIntpre& nameServer_nodes_map);
void QueryResolver(Zone& z, InterpreterGraph& g, IntpVD& v, NameServerIntpre& nameServerZoneMap);
void BuildInterpretationGraph(EC& query, InterpreterGraphWrapper& intGraph_wrapper);
