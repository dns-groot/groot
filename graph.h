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

using namespace std;

//Label Graph
struct labelVertex { 
	string name; 
	std::bitset<rr_type::N> rrTypesPresent;
	int16_t	len = 0;
};

enum edge_type {
	normal = 1,
	cname = 2,
	dname = 3
};

struct labelEdge { 
	edge_type type; 
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, labelVertex, labelEdge > labelGraph;

//Some typedefs for simplicity
typedef boost::graph_traits<labelGraph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<labelGraph>::edge_descriptor edge_t;
typedef boost::graph_traits<labelGraph>::vertex_iterator vertex_iter;
typedef boost::graph_traits<labelGraph>::edge_iterator edge_iter;

vector<std::string> getLabels(string name);
void label_graph_builder(vector<ResourceRecord>, labelGraph&, vertex_t);
void printGraph(labelGraph& g);


//Equivalence Classes
struct EC {
	boost::optional<std::vector<std::string>> excluded;
	string	name;
	std::bitset<rr_type::N> rrTypes;
	
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& name;
		ar& rrTypes;
		ar& excluded;
	}
};

typedef struct EC EC;

void ECgen(labelGraph& g, vertex_t root, vector<EC>& allQueries);

