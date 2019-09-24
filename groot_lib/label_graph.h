#include <boost/algorithm/string.hpp>
#include "graph.h"
#include "interpreter.h"
#include "properties.h"


std::size_t hash_value(Label const& l);

LabelMap ConstructLabelMap(const LabelGraph& g, VertexDescriptor node);
VertexDescriptor GetClosestEncloser(const LabelGraph& g, VertexDescriptor root, vector<Label> labels, int& index);
VertexDescriptor AddNodes(LabelGraph& g, VertexDescriptor closetEncloser, ResourceRecord* rr, vector<Label> labels, int& index);
void LabelGraphBuilder(ResourceRecord& record, LabelGraph& g, const VertexDescriptor root);
void LabelGraphBuilder(vector<ResourceRecord>& rrs, LabelGraph& g, const VertexDescriptor root);
boost::optional<std::bitset<RRType::N>> CNAMELookup(const LabelGraph& g, VertexDescriptor  start, std::unordered_set<VertexDescriptor> visited_nodes);
void DFSVisit(LabelGraph& g, VertexDescriptor  start, vector<Label> parentDomainName, vector<EC>& allQueries, bool skipLabel);
void ECGenerator(LabelGraph& g, VertexDescriptor root, vector<EC>& allQueries);