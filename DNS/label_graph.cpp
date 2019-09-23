#include <boost/algorithm/string.hpp>
#include "graph.h"
#include "interpreter.h"
#include "properties.h"

std::map<VertexDescriptor, LabelMap> gDomainChildLabelMap;


std::size_t hash_value(Label const& l)
{
	boost::hash<boost::flyweight<std::string, boost::flyweights::no_locking, boost::flyweights::no_tracking>> hasher;
	return hasher(l.n);
}

LabelMap ConstructLabelMap(const LabelGraph& g, VertexDescriptor node) {
	LabelMap m;
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(node, g))) {
		if (g[edge].type == normal) {
			m[g[edge.m_target].name] = edge.m_target;
		}
	}
	return m;
}


VertexDescriptor GetClosestEncloser(const LabelGraph& g, VertexDescriptor root, vector<Label> labels, int& index) {
	VertexDescriptor closestEncloser = root;
	if (labels.size() == index) {
		return closestEncloser;
	}
	if (out_degree(closestEncloser, g) > kHashMapThreshold) {
		if (gDomainChildLabelMap.find(closestEncloser) == gDomainChildLabelMap.end()) {
			gDomainChildLabelMap[closestEncloser] = ConstructLabelMap(g, closestEncloser);
		}
		LabelMap& m = gDomainChildLabelMap.find(closestEncloser)->second;
		auto it = m.find(labels[index]);
		if (it != m.end()) {
			closestEncloser = it->second;
			index++;
			return GetClosestEncloser(g, closestEncloser, labels, index);
		}
	}
	else {
		for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closestEncloser, g))) {
			if (g[edge].type == normal) {
				if (g[edge.m_target].name == labels[index]) {
					closestEncloser = edge.m_target;
					index++;
					return GetClosestEncloser(g, closestEncloser, labels, index);
				}
			}
		}
	}
	return closestEncloser;
}


VertexDescriptor AddNodes(LabelGraph& g, VertexDescriptor closetEncloser, ResourceRecord* rr, vector<Label> labels, int& index) {
	
	for (int i = index; i < labels.size(); i++) {
		VertexDescriptor u = boost::add_vertex(g);
		g[u].name = labels[i];
		EdgeDescriptor e; bool b;
		boost::tie(e, b) = boost::add_edge(closetEncloser, u, g);
		if (!b) {
			cout << "Unable to add edge" << endl;
			exit(EXIT_FAILURE);
		}
		g[e].type = normal;
		//Only the first closestEncloser might have a map. For other cases closestEncloser will take care.
		if (gDomainChildLabelMap.find(closetEncloser) != gDomainChildLabelMap.end()) {
			LabelMap& m = gDomainChildLabelMap.find(closetEncloser)->second;
			m[labels[i]] = u;
		}
		closetEncloser = u;
	}
	if (rr != NULL) {
		g[closetEncloser].rrTypesPresent.set(rr->get_type());
	}
	return closetEncloser;
}

void LabelGraphBuilder(ResourceRecord& record, LabelGraph& g, const VertexDescriptor root) {
	
	if (record.get_type() != RRType::N) {
		int index = 0;
		VertexDescriptor closetEncloser = GetClosestEncloser(g, root, record.get_name(), index);
		VertexDescriptor mainNode = AddNodes(g, closetEncloser, &record, record.get_name(), index);
		if (record.get_type() == RRType::CNAME || record.get_type() == RRType::DNAME) {
			vector<Label> labels = GetLabels(record.get_rdata());
			index = 0;
			closetEncloser = GetClosestEncloser(g, root, labels, index);
			VertexDescriptor secondNode = AddNodes(g, closetEncloser, NULL, labels, index);
			EdgeDescriptor e; bool b;
			boost::tie(e, b) = boost::add_edge(mainNode, secondNode, g);
			if (!b) {
				cout << "Unable to add edge" << endl;
				exit(EXIT_FAILURE);
			}
			if (record.get_type() == RRType::CNAME) {
				g[e].type = cname;
			}
			if (record.get_type() == RRType::DNAME) {
				g[e].type = dname;
			}
		}
	}
}

void LabelGraphBuilder(vector<ResourceRecord>& rrs, LabelGraph& g, const VertexDescriptor root) {

	//Assumption : All RR's have the owner names, CNAMEs and DNAMEs as Fully Quantified Domain Name.
	for (auto& record : rrs)
	{
		LabelGraphBuilder(record, g, root);
	}
}

std::optional<std::bitset<RRType::N>> CNAMELookup(const LabelGraph& g, VertexDescriptor  start, std::unordered_set<VertexDescriptor> visited_nodes) {
	VertexDescriptor current = start;
	auto before = visited_nodes.size();
	visited_nodes.insert(current);
	if (before == visited_nodes.size()) {
		//cout << "CNAME Loop Detected\n";
		return {};
	}
	std::bitset<RRType::N> types;
	bool found = false;
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(current, g))) {
		if (g[edge].type == cname) {
			found = true;
			if (g[edge.m_target].rrTypesPresent[RRType::CNAME] == 1) {
				std::optional<std::bitset<RRType::N>> typesReturned = CNAMELookup(g, edge.m_target, visited_nodes);
				if (typesReturned) {
					types = types | typesReturned.value();
				}
			}
			else {
				types = types | g[edge.m_target].rrTypesPresent;
			}
		}
	}
	if (types.count() > 0) {
		return types;
	}
	return {};
}


void DFSVisit(LabelGraph& g, VertexDescriptor  start, vector<Label> parentDomainName, vector<EC>& allQueries, bool skipLabel) {
	
	int len = 0;
	for (Label l : parentDomainName) {
		len += l.get().length() + 1;
	}
	if (len == kMaxDomainLength) {
		return;
	}
	int16_t beforeLen = g[start].len;
	if (len > 0 && len == beforeLen) {
		//cout<< "DNAME Loop Detected Starting with "<< g[start].name << endl;
		return;
	}
	Label nodeLabel = g[start].name;
	vector<Label> name;
	if (nodeLabel.get() == ".") {
		//Empty name vector implies root
	}
	else if (parentDomainName.size() == 0) {
		name.push_back(nodeLabel);
	}
	else if (skipLabel) {
		//DNAME can not occur at the root.
		name = parentDomainName;
	}
	else {
		name = parentDomainName;
		name.push_back(nodeLabel);
	}
	int nodeLen = 0;
	for (Label l : name) {
		nodeLen += l.get().length() + 1;
	}
	g[start].len = nodeLen;
	std::vector<Label> childrenLabels;
	std::optional<std::bitset<RRType::N>> wildcardTypes;
	std::optional<std::bitset<RRType::N>> cnameTypes;
	VertexDescriptor wildcardNode{};
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(start, g))) {
		if (g[edge].type == normal) {
			if (g[edge.m_target].name.get() == "*") {
				wildcardNode = edge.m_target;
				// If the child wildcard is a CNAME then perform the CNAME lookup to get the types possible
				if (g[edge.m_target].rrTypesPresent[RRType::CNAME] == 1) {
					auto types = CNAMELookup(g, edge.m_target, std::unordered_set<VertexDescriptor> ());
					if (types) {
						wildcardTypes = make_optional(g[edge.m_target].rrTypesPresent | types.value());
					}
				}
				else {
					wildcardTypes = make_optional(g[edge.m_target].rrTypesPresent);
				}
			}
			else {
				childrenLabels.push_back(g[edge.m_target].name);
			}
			DFSVisit(g, edge.m_target, name, allQueries, false);
		}
		if (g[edge].type == cname) {
			cnameTypes = CNAMELookup(g, start, std::unordered_set<VertexDescriptor>());
		}
		if (g[edge].type == dname) {
			DFSVisit(g, edge.m_target, name, allQueries, true);
		}
	}
	g[start].len = beforeLen;
	if (nodeLabel.get() == "*") {
		//Taken care by parent
	}
	else if (!skipLabel) {
		// Node and the types present at it if the node is not skipped.
		if (g[start].rrTypesPresent.count() > 0) {
			EC present;
			present.name = name;
			present.rrTypes = g[start].rrTypesPresent;
			if (cnameTypes) {
				present.rrTypes |= cnameTypes.value();
			}
			present.id = start;
			g[start].ECindicies.push_back(allQueries.size());
			allQueries.push_back(present);
		}
		if (g[start].rrTypesPresent.count() < RRType::N) {
			// Node and not the types present at it
			EC absent;
			absent.name = name;
			absent.rrTypes = g[start].rrTypesPresent;
			if (cnameTypes) {
				absent.rrTypes |= cnameTypes.value();
			}
			absent.id = start;
			absent.rrTypes.flip();
			g[start].ECindicies.push_back(allQueries.size());
			allQueries.push_back(absent);
		}
	}

	//If there is a wildcard child. (Wildcard child cannot handle itself and has to be taken care by the parent)
	if (wildcardTypes) {
		std::vector<Label> copied = childrenLabels;
		if (wildcardTypes.value().count() > 0) {
			// Cases matching wildcard
			EC wildCardMatch;
			wildCardMatch.id = wildcardNode;
			wildCardMatch.name = name;
			wildCardMatch.rrTypes = wildcardTypes.value();
			wildCardMatch.excluded = boost::make_optional(std::move(childrenLabels));
			g[wildcardNode].ECindicies.push_back(allQueries.size());
			allQueries.push_back(wildCardMatch);
		}

		if (wildcardTypes.value().count() < RRType::N) {
			// All other non-existent queries
			EC nonExistent;
			nonExistent.id = wildcardNode;
			nonExistent.name = name;
			nonExistent.rrTypes = wildcardTypes.value();
			nonExistent.rrTypes.flip();
			//copied.push_back("*"); // TODO: Check this line
			nonExistent.excluded = boost::make_optional(std::move(copied));
			g[wildcardNode].ECindicies.push_back(allQueries.size());
			allQueries.push_back(nonExistent);
		}
	}
	else {
		// Any query for a non-child name
		EC nonExistent;
		nonExistent.id = start;
		g[start].ECindicies.push_back(allQueries.size());
		nonExistent.name = name;
		nonExistent.rrTypes.flip();
		nonExistent.excluded = boost::make_optional(std::move(childrenLabels));
		allQueries.push_back(nonExistent);
	}
}

void ECGenerator(LabelGraph& g, VertexDescriptor root, vector<EC>& allQueries) {
	DFSVisit(g, root, vector<Label>{}, allQueries, false);
}
