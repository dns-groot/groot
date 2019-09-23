#include <boost/algorithm/string.hpp>
#include "graph.h"




vertex_t getClosestEncloser(labelGraph& g, vertex_t root, vector<std::string> labels, int& index) {

	vertex_t closetEncloser = root;
	if (labels.size() == index) {
		return closetEncloser;
	}

	for (edge_t edge : boost::make_iterator_range(out_edges(closetEncloser, g))) {
		if (g[edge].type == normal) {
			if (g[edge.m_target].name.compare(labels[index]) == 0) {
				closetEncloser = edge.m_target;
				index++;
				return getClosestEncloser(g, closetEncloser, labels, index);
			}
		}

	}
	return closetEncloser;
}


vertex_t addNodes(labelGraph& g, vertex_t closetEncloser, ResourceRecord* rr, vector<std::string> labels, int& index) {

	for (int i = index; i < labels.size(); i++) {
		vertex_t u = boost::add_vertex(g);
		g[u].name = labels[i];
		edge_t e; bool b;
		boost::tie(e, b) = boost::add_edge(closetEncloser, u, g);
		if (!b) {
			cout << "Unable to add edge" << endl;
			exit(EXIT_FAILURE);
		}
		g[e].type = normal;
		closetEncloser = u;
	}
	if (rr != NULL) {
		g[closetEncloser].rrTypesPresent.set(rr->GetType());
	}
	return closetEncloser;
}

vector<std::string> getLabels(string name) {
	vector<std::string> labels;
	// boost::algorithm::split(labels, name, boost::is_any_of(".")); // Avoiding this for the case where . is written with \. and root zone.
	string previous = "";
	for (auto it = name.begin(); it < name.end(); ++it) {
		if (*it == '.' && previous.length() > 0) {
			if (previous.back() == '\\') {
				previous += *it;
			}
			else {
				labels.push_back(previous);
				previous = "";
			}
		}
		else {
			previous += *it;
		}
	}
	std::reverse(labels.begin(), labels.end());
	return labels;
}

void printGraph(labelGraph& g) {
	for (auto vd : g.vertex_set())
	{
		std::cout << "vertex " << g[vd].name << ": " << g[vd].rrTypesPresent << ":";
		for (edge_t edge : boost::make_iterator_range(out_edges(vd, g))) {
			std::cout << edge << "," << g[edge].type << "  ";
		}
		std::cout << "\n";
	}
}

void label_graph_builder(vector<ResourceRecord> rrs, labelGraph& g, vertex_t root) {

	//Assumption : All RR's have the owner names, CNAMEs and DNAMEs as Fully Qunatified Domain Name.
	for (auto& record : rrs)
	{
		if (record.GetType() != rr_type::N) {
			string name = record.GetName();
			vector<std::string> labels = getLabels(name);
			int index = 0;
			vertex_t closetEncloser = getClosestEncloser(g, root, labels, index);
			vertex_t mainNode = addNodes(g, closetEncloser, &record, labels, index);

			if (record.GetType() == rr_type::CNAME || record.GetType() == rr_type::DNAME) {
				labels = getLabels(record.GetRData());
				index = 0;
				closetEncloser = getClosestEncloser(g, root, labels, index);
				vertex_t secondNode = addNodes(g, closetEncloser, NULL, labels, index);
				edge_t e; bool b;
				boost::tie(e, b) = boost::add_edge(mainNode, secondNode, g);
				if (!b) {
					cout << "Unable to add edge" << endl;
					exit(EXIT_FAILURE);
				}
				if (record.GetType() == rr_type::CNAME) {
					g[e].type = cname;
				}
				if (record.GetType() == rr_type::DNAME) {
					g[e].type = dname;
				}
			}
		}
	}
	
}

std::optional<std::bitset<rr_type::N>>  CNAME_lookup(labelGraph& g, vertex_t  start, string parent) {
	// Has to be changed to include CNAME Loops as they might be from differentr name servers
	std::unordered_set<vertex_t> visited_nodes;
	vertex_t current = start;
	while (true) {
		auto before = visited_nodes.size();
		visited_nodes.insert(current);
		if (before == visited_nodes.size()) {
			cout << "CNAME Loop Detected starting at" << g[start].name + "." + parent << endl;
			return {};
		}
		bool found = false;
		for (edge_t edge : boost::make_iterator_range(out_edges(current, g))) {
			if (g[edge].type == cname) {
				found = true;
				if (g[edge.m_target].rrTypesPresent[rr_type::CNAME] == 1) {
					//continue the search
					current = edge.m_target;
					break;
				}
				else {
					return g[edge.m_target].rrTypesPresent;
				}
			}
		}
		if (!found) {
			return {};
		}
	}
}


void DFS_VISIT(labelGraph& g, vertex_t  start, string parentDomainName, vector<EC>& allQueries, bool skipLabel) {
	
	if (parentDomainName.length() == 254) {
		return;
	}
	int16_t beforeLen = g[start].len;
	if (parentDomainName.length() > 0 && parentDomainName.length() == beforeLen) {
		cout<< "DNAME Loop Detected Starting with "<< g[start].name << endl;
		return;
	}
	string nodeLabel = g[start].name;
	string name;
	if (nodeLabel == ".") {
		name = ".";
	}
	else if (parentDomainName == ".") {
		name = nodeLabel + ".";
	}
	else if (skipLabel) {
		//DNAME can not occur at the root.
		name = parentDomainName;
	}
	else {
		name = nodeLabel + "." + parentDomainName;
	}
	g[start].len = name.length();
	std::vector<std::string> childrenLabels;
	std::optional<std::bitset<rr_type::N>> wildcardTypes;
	std::optional<std::bitset<rr_type::N>> cnameTypes;

	for (edge_t edge : boost::make_iterator_range(out_edges(start, g))) {
		if (g[edge].type == normal) {
			if (g[edge.m_target].name == "*") {
				// If the child wildcard is a CNAME then perform the CNAME lookup to get the types possible
				if (g[edge.m_target].rrTypesPresent[rr_type::CNAME] == 1) {
					auto types = CNAME_lookup(g, edge.m_target, name); // name may not be correct when skipLabel is true.
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
			DFS_VISIT(g, edge.m_target, name, allQueries, false);
		}
		if (g[edge].type == cname) {
			cnameTypes = CNAME_lookup(g, start, parentDomainName);
		}
		if (g[edge].type == dname) {
			DFS_VISIT(g, edge.m_target, name, allQueries, true);
		}
	}
	g[start].len = beforeLen;
	if (nodeLabel == "*") {
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
			allQueries.push_back(present);
		}
		if (g[start].rrTypesPresent.count() < rr_type::N) {
			// Node and not the types present at it
			EC absent;
			absent.name = name;
			absent.rrTypes = g[start].rrTypesPresent;
			if (cnameTypes) {
				absent.rrTypes |= cnameTypes.value();
			}
			absent.rrTypes.flip();
			allQueries.push_back(absent);
		}
	}

	//If there is a wildcard child. (Wildcard child cannot handle itself and has to be taken care by the parent)
	if (wildcardTypes) {
		std::vector<std::string> copied = childrenLabels;
		if (wildcardTypes.value().count() > 0) {
			// Cases matching wildcard
			EC wildCardMatch;
			wildCardMatch.name = name;
			wildCardMatch.rrTypes = wildcardTypes.value();
			wildCardMatch.excluded = boost::make_optional(std::move(childrenLabels));
			allQueries.push_back(wildCardMatch);
		}

		if (wildcardTypes.value().count() < rr_type::N) {
			// All other non-existent queries
			EC nonExistent;
			nonExistent.name = name;
			nonExistent.rrTypes = wildcardTypes.value();
			nonExistent.rrTypes.flip();
			copied.push_back("*");
			nonExistent.excluded = boost::make_optional(std::move(copied));
			allQueries.push_back(nonExistent);
		}
	}
	else {
		// Any query for a non-child name
		EC nonExistent;
		nonExistent.name = name;
		nonExistent.rrTypes.flip();
		nonExistent.excluded = boost::make_optional(std::move(childrenLabels));
	}
}

void ECgen(labelGraph& g, vertex_t root, vector<EC>& allQueries) {

	auto n = boost::num_vertices(g);
	DFS_VISIT(g, root, "", allQueries, false);
	cout << allQueries.size() << endl;
}