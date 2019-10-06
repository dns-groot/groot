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

using namespace std;

#define kHashMapThreshold 1000

//Label Graph
struct LabelVertex { 
	Label name; 
	int16_t	len = -1;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& name;
		ar& rrTypesPresent;
		ar& len;
		ar& ECindicies;
	}
};

enum EdgeType {
	normal = 1,
	dname = 2
};

struct LabelEdge { 
	EdgeType type; 
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& type;
	}
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, LabelVertex, LabelEdge > LabelGraph;

//Some typedefs for simplicity
typedef boost::graph_traits<LabelGraph>::vertex_descriptor VertexDescriptor;
typedef boost::graph_traits<LabelGraph>::edge_descriptor EdgeDescriptor;
typedef boost::graph_traits<LabelGraph>::vertex_iterator VertexIterator;
typedef boost::graph_traits<LabelGraph>::edge_iterator EdgeIterator;

typedef boost::unordered_map<Label, VertexDescriptor> LabelMap;
extern std::map<VertexDescriptor, LabelMap> gDomainChildLabelMap;

std::size_t hash_value(Label const& l);
void LabelGraphBuilder(vector<ResourceRecord>&, LabelGraph&, const VertexDescriptor);
void LabelGraphBuilder(ResourceRecord&, LabelGraph&, const VertexDescriptor);

//Equivalence Classes
struct EC {
	boost::optional<std::vector<Label>> excluded;
	vector<Label> name;
	std::bitset<RRType::N> rrTypes;

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