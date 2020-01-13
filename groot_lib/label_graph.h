#include <boost/algorithm/string.hpp>
#include "graph.h"
#include "interpreter.h"
#include "properties.h"


std::size_t hash_value(Label const& l);

LabelMap ConstructLabelMap(const LabelGraph& g, VertexDescriptor node);
VertexDescriptor GetClosestEncloser(const LabelGraph& g, VertexDescriptor root, vector<Label> labels, int& index);
VertexDescriptor AddNodes(LabelGraph& g, VertexDescriptor closetEncloser, vector<Label> labels, int& index);
void LabelGraphBuilder(ResourceRecord& record, LabelGraph& g, const VertexDescriptor root, int& zoneId, int& zoneVertexId);
//void LabelGraphBuilder(vector<ResourceRecord>& rrs, LabelGraph& g, const VertexDescriptor root);