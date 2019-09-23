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
#include "RR.h"
#include "graph.h"
#include "zone.h"


struct intVertex {
	std::string ns;
	EC query;
	boost::optional<vector<ResourceRecord>> answer;
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, intVertex> intGraph;

//Some typedefs for simplicity
typedef boost::graph_traits<intGraph>::vertex_descriptor intVertex_t;
typedef boost::graph_traits<intGraph>::edge_descriptor intEdge_t;


extern std::map<string, std::vector<zone>> nameServer_Zone_Map;

void build_interpreter_graph(string ns, EC& query, intGraph& g);