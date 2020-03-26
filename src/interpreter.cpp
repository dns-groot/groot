#include "interpreter.h"

template <class NSMap, class QueryMap, class AnswerMap>
class node_writer {
public:
	node_writer(NSMap ns, QueryMap q, AnswerMap a) : nsm(ns), qm(q), am(a) {}
	template <class Vertex>
	void operator()(ostream& out, const Vertex& v) const {
		auto nameServer = get(nsm, v);
		auto query = get(qm, v);
		auto answer = get(am, v);
		//string label = "[label=\"NS: " + nameServer + " \\n Q: " + query.name + " T:"+ RRTypesToString(query.rrTypes) + "  \\n A:";
		string queryName = "\\n Q:";
		if (query.excluded) {
			queryName += "~{}.";
		}
		string label = "[label=\"NS: " + nameServer + queryName + LabelUtils::LabelsToString(query.name) + " T:" + TypeUtils::TypesToString(query.rrTypes) + "  \\n A:";
		std::bitset<RRType::N> answerTypes;
		if (answer) {
			for (auto r : answer.get()) {
				label += ReturnTagToString(std::get<0>(r));
			}
		}
		//label += RRTypesToString(answerTypes) + "\"]";
		label += "\"]";
		if (nameServer == "") {
			label = label + " [shape=diamond]";
		}
		out << label;
	}
private:
	NSMap nsm;
	QueryMap qm;
	AnswerMap am;
};

template <class NSMap, class QueryMap, class AnswerMap>
inline node_writer<NSMap, QueryMap, AnswerMap>
make_node_writer(NSMap ns, QueryMap q, AnswerMap a) {
	return node_writer<NSMap, QueryMap, AnswerMap>(ns, q, a);
}


template <class EdgeMap>
class edge_writer {
public:
	edge_writer(EdgeMap w) : wm(w) {}
	template <class Edge>
	void operator()(ostream& out, const Edge& e) const {
		auto type = get(wm, e);
		if (!type) {
			out << "[color=black]";
		}
		else {
			out << "[color=red]";
		}
	}
private:
	EdgeMap wm;
};

template <class EdgeMap>
inline edge_writer<EdgeMap>
make_edge_writer(EdgeMap w) {
	return edge_writer<EdgeMap>(w);
}


void GenerateDotFileInterpreter(string outputfile, InterpreterGraph& g) {
	std::ofstream dotfile(outputfile);
	write_graphviz(dotfile, g, make_node_writer(boost::get(&InterpreterVertex::ns, g), boost::get(&InterpreterVertex::query, g), boost::get(&InterpreterVertex::answer, g)), make_edge_writer(boost::get(&InterpreterEdge::intermediateQuery, g)));
}

EC ProcessDNAME(ResourceRecord& record, EC& query) {
	int i = 0;
	for (auto& l : record.get_name()) {
		if (!(l == query.name[i])) {
			Logger->critical(fmt::format("interpreter.cpp (ProcessDNAME) - Query name is not a sub-domain of the DNAME record"));
			exit(EXIT_FAILURE);
		}
		i++;
	}
	vector<Label> rdataLabels = LabelUtils::StringToLabels(record.get_rdata());
	vector<Label> namelabels = query.name;
	for (; i < namelabels.size(); i++) {
		rdataLabels.push_back(namelabels[i]);
	}
	EC newQuery;
	newQuery.excluded = query.excluded;
	newQuery.rrTypes = query.rrTypes;
	newQuery.name = std::move(rdataLabels);
	return newQuery;
}

EC ProcessCNAME(ResourceRecord& record, EC& query) {
	EC newQuery;
	newQuery.rrTypes = query.rrTypes;
	newQuery.rrTypes.reset(RRType::CNAME);
	newQuery.name = LabelUtils::StringToLabels(record.get_rdata());
	return newQuery;
}

boost::optional<int> GetRelevantZone(string ns, EC& query) {
	auto it = gNameServerZoneMap.find(ns);
	auto queryLabels = query.name;
	if (it != gNameServerZoneMap.end()) {
		bool found = false;
		int max = -1;
		int bestMatch = 0;
		for (auto zid : it->second) {
			int i = 0;
			bool valid = true;
			for (Label& l : gZoneIdToZoneMap.find(zid)->second.origin) {
				if (i >= queryLabels.size() || l.n != queryLabels[i].n) {
					valid = false;
					break;
				}
				i++;
			}
			if (valid) {
				found = true;
				if (i > max) {
					max = i;
					bestMatch = zid;
				}
			}
		}
		if (found)	return bestMatch;
		else return {};
	}
	else {
		return {};
	}
}

IntpVD SideQuery(NameServerIntpre& nameServerNodesMap, InterpreterGraph& intG, EC& query) {
	auto it = nameServerNodesMap.find("");
	if (it == nameServerNodesMap.end()) {
		nameServerNodesMap.insert(std::pair<string, std::vector<IntpVD>>("", std::vector<IntpVD>()));
	}
	else {
		std::vector<IntpVD> existingNodes = it->second;
		for (auto n : existingNodes) {
			if (CheckQueryEquivalence(query, intG[n].query)) {
				return n;
			}
		}
	}
	//Add a dummy vertex as the start node over the top Name Servers
	IntpVD dummy = boost::add_vertex(intG);
	intG[dummy].ns = "";
	intG[dummy].query = query;
	it = nameServerNodesMap.find("");
	if (it != nameServerNodesMap.end()) {
		it->second.push_back(dummy);
	}
	else {
		Logger->critical(fmt::format("interpreter.cpp (SideQuery) - Unable to insert into a nameServerNodesMap"));
		std::exit(EXIT_FAILURE);
	}
	StartFromTop(intG, dummy, query, nameServerNodesMap);
	return dummy;
}

vector<ResourceRecord> SeparateGlueRecords(vector<ResourceRecord> records) {
	//If there is an NS record only glue records can exist 
	vector<Label> domain;
	vector<ResourceRecord> glueRecords;
	for (auto& record : records) {
		if (record.get_type() == RRType::NS) {
			domain = record.get_name();
			break;
		}
	}
	vector<int> deleteIndicies;
	if (domain.size() > 0) {
		int i = 0;
		for (auto& record : records) {
			if (record.get_name() != domain) {
				glueRecords.push_back(record);
				deleteIndicies.push_back(i);
			}
			i++;
		}
		std::sort(deleteIndicies.begin(), deleteIndicies.end(), std::greater<>());
		for (auto& index : deleteIndicies) {
			records.erase(records.begin() + index);
		}
	}
	return glueRecords;
}

vector<tuple<ResourceRecord, vector<ResourceRecord>>> PairGlueRecords(vector<ResourceRecord>& records) {
	//vector<ResourceRecord> glueRecords;
	//vector<ResourceRecord> nsRecords;
	//for (auto& r : records) {
	//	if (r.get_type() == RRType::NS) {
	//		nsRecords.push_back(r);
	//	}
	//	else if (r.get_type() == RRType::A || r.get_type() == RRType::AAAA) {
	//		glueRecords.push_back(r);
	//	}
	//	else {
	//		cout << "PairGlueRecords: Found types other than NS and A";
	//	}
	//}
	//vector<tuple<ResourceRecord, vector<ResourceRecord>>> pairs;
	//for (auto& ns : nsRecords) {
	//	vector<ResourceRecord> matched;
	//	for (auto& glue : glueRecords) {
	//		if (ns.get_rdata() == LabelsToString(glue.get_name())) {
	//			matched.push_back(std::move(glue));
	//		}
	//	}
	//	pairs.push_back(std::make_tuple(std::move(ns), std::move(matched)));
	//}
	//

	vector<tuple<ResourceRecord, vector<ResourceRecord>>> pairs;
	for (int i = 0; i < records.size(); i++) {
		if (records[i].get_type() == RRType::NS) {
			vector<ResourceRecord> matched;
			for (int j = i + 1; j < records.size(); j++) {
				if (records[j].get_type() == RRType::A || records[j].get_type() == RRType::AAAA) {
					matched.push_back(records[j]);
				}
				else {
					pairs.push_back(std::make_tuple(std::move(records[i]), std::move(matched)));
					i = j - 1;
					break;
				}
			}
			if (matched.size()) {
				pairs.push_back(std::make_tuple(std::move(records[i]), std::move(matched)));
			}
		}
	}
	return pairs;

}

void CNAME_DNAME_SameServer(InterpreterGraph& g, IntpVD& v, NameServerIntpre& nameServerZoneMap, EC& newQuery) {
	//Search for relevant zone at the same name server first
	boost::optional<int> start = GetRelevantZone(g[v].ns, newQuery);
	if (start) {
		boost::optional<IntpVD> node = InsertNode(nameServerZoneMap, g, g[v].ns, newQuery, v, {});
		//If there was no same query earlier to this NS then continue the querying process.
		if (node) {
			QueryResolver(gZoneIdToZoneMap.find(start.get())->second, g, node.get(), nameServerZoneMap);
		}
	}
	else {
		//Start from the top Zone file 
		StartFromTop(g, v, newQuery, nameServerZoneMap);
	}
}

void NS_SubRoutine(InterpreterGraph& g, IntpVD& v, NameServerIntpre& nameServerZoneMap, string& newNS, boost::optional<IntpVD> edgeQuery) {

	boost::optional<IntpVD> node = InsertNode(nameServerZoneMap, g, newNS, g[v].query, v, edgeQuery);
	if (node) {
		auto it = gNameServerZoneMap.find(newNS);
		if (it != gNameServerZoneMap.end()) {
			boost::optional<int> start = GetRelevantZone(newNS, g[v].query);
			if (start) {
				QueryResolver(gZoneIdToZoneMap.find(start.get())->second, g, node.get(), nameServerZoneMap);
			}
			else {
				//Path terminates - No relevant zone file available from the NS.
				vector<ZoneLookUpAnswer> answer;
				answer.push_back(std::make_tuple(ReturnTag::REFUSED, g[node.get()].query.rrTypes, vector<ResourceRecord> {}));
				g[node.get()].answer = answer;
			}
		}
		else {
			//Path terminates -  The newNS not found.
			vector<ZoneLookUpAnswer> answer;
			answer.push_back(std::make_tuple(ReturnTag::NSNOTFOUND, g[node.get()].query.rrTypes, vector<ResourceRecord> {}));
			g[node.get()].answer = answer;
		}

	}
}

void QueryResolver(Zone& z, InterpreterGraph& g, IntpVD& v, NameServerIntpre& nameServerZoneMap) {

	bool completeMatch = false;
	g[v].answer = QueryLookUpAtZone(z, g[v].query, completeMatch);
	if (g[v].answer) {
		vector<ZoneLookUpAnswer> answers = g[v].answer.get();
		if (answers.size() > 0) {
			for (auto& answer : answers) {
				ReturnTag& ret = std::get<0>(answer);
				if (ret == ReturnTag::ANS) {
					// Found Answer no further queries.
				}
				else if (ret == ReturnTag::NX) {
					// Non-existent domain or Type not found Case
				}
				else if (ret == ReturnTag::REWRITE) {
					ResourceRecord& record = std::get<2>(answer)[0];
					//It will always be of size 1
					//CNAME Case
					if (record.get_type() == RRType::CNAME) {
						EC newQuery = ProcessCNAME(record, g[v].query);
						CNAME_DNAME_SameServer(g, v, nameServerZoneMap, newQuery);
					}
					else {
						//DNAME Case
						EC newQuery = ProcessDNAME(record, g[v].query);
						CNAME_DNAME_SameServer(g, v, nameServerZoneMap, newQuery);
					}
				}
				else if (ret == ReturnTag::REF) {
					// Referral to other NS case.
					vector<tuple<ResourceRecord, vector<ResourceRecord>>> pairs = PairGlueRecords(std::get<2>(answer));
					for (auto& pair : pairs) {
						string newNS = std::get<0>(pair).get_rdata();
						vector<ResourceRecord> glueRecords = std::get<1>(pair);
						//Either the Glue records have to exist or the referral to a topNameServer
						if (glueRecords.size() || (std::find(gTopNameServers.begin(), gTopNameServers.end(), newNS) != gTopNameServers.end())) {
							NS_SubRoutine(g, v, nameServerZoneMap, newNS, {});
						}
						else {
							// Have to query for the IP address of NS
							EC nsQuery;
							nsQuery.name = LabelUtils::StringToLabels(newNS);
							nsQuery.rrTypes[RRType::A] = 1;
							nsQuery.rrTypes[RRType::AAAA] = 1;
							IntpVD nsStart = SideQuery(nameServerZoneMap, g, nsQuery);
							//TODO: Check starting from this nsStart node if we got the ip records
							NS_SubRoutine(g, v, nameServerZoneMap, newNS, nsStart);
						}
					}
				}
			}
		}
		else {
			// No records found - NXDOMAIN. The current path terminates.
		}
	}
	else {
		// This path terminates - On the assumption that you would have come to this NS by some referrral and if the referral is wrong then the path should terminate.
	}
}

bool CheckQueryEquivalence(EC& query, EC& nodeQuery) {
	if (query.name != nodeQuery.name) {
		return false;
	}
	for (int i = 0; i < RRType::N; i++) {
		if (query.rrTypes[i] == 1 && nodeQuery.rrTypes[i] != 1) {
			return false;
		}
	}
	if (query.excluded && !nodeQuery.excluded) {
		return false;
	}
	if (!query.excluded && nodeQuery.excluded) {
		return false;
	}
	return true;
}

boost::optional<IntpVD> InsertNode(NameServerIntpre& nameServerNodesMap, InterpreterGraph& intG, string ns, EC query, IntpVD edgeStart, boost::optional<IntpVD> edgeQuery) {
	//First checks if a node exists in the graph with NS = ns and Query = query
	//If it exists, then it add an edge from the edgeStart to found node and returns {}
	//Else Creates a new node and adds an edge and returns the new node
	auto it = nameServerNodesMap.find(ns);
	if (it == nameServerNodesMap.end()) {
		nameServerNodesMap.insert(std::pair<string, std::vector<IntpVD>>(ns, std::vector<IntpVD>()));
	}
	else {
		std::vector<IntpVD> existingNodes = it->second;
		for (auto n : existingNodes) {
			if (CheckQueryEquivalence(query, intG[n].query)) {
				IntpED e; bool b;
				/*if (edgeStart != n) {*/
				boost::tie(e, b) = boost::add_edge(edgeStart, n, intG);
				if (!b) {
					Logger->critical(fmt::format("interpreter.cpp (InsertNode) - Unable to add edge to a interpretation graph"));
					std::exit(EXIT_FAILURE);
				}
				if (edgeQuery) {
					intG[e].intermediateQuery = boost::make_optional(edgeQuery.get());
				}
				//}
				return {};
			}
		}
	}
	it = nameServerNodesMap.find(ns);
	if (it != nameServerNodesMap.end()) {
		IntpVD v = boost::add_vertex(intG);

		intG[v].ns = ns;
		intG[v].query = query;
		IntpED e; bool b;
		boost::tie(e, b) = boost::add_edge(edgeStart, v, intG);
		if (!b) {
			Logger->critical(fmt::format("interpreter.cpp (InsertNode) - Unable to add edge to a interpretation graph"));
			std::exit(EXIT_FAILURE);
		}
		if (edgeQuery) {
			intG[e].intermediateQuery = boost::make_optional(edgeQuery.get());
		}
		it->second.push_back(v);
		return v;
	}
	else {
		Logger->critical(fmt::format("interpreter.cpp (InsertNode) -Unable to insert into a nameServerNodesMap"));
		std::exit(EXIT_FAILURE);
	}

}

void StartFromTop(InterpreterGraph& intG, IntpVD edgeStartNode, EC& query, NameServerIntpre& nameServerNodesMap) {
	for (string ns : gTopNameServers) {
		// ns exists in the database.
		boost::optional<int> start = GetRelevantZone(ns, query);
		boost::optional<IntpVD> node = InsertNode(nameServerNodesMap, intG, ns, query, edgeStartNode, {});
		if (start && node) {
			QueryResolver(gZoneIdToZoneMap.find(start.get())->second, intG, node.get(), nameServerNodesMap);
		}
		else if (node && !start) {
			vector<ZoneLookUpAnswer> answer;
			answer.push_back(std::make_tuple(ReturnTag::REFUSED, intG[node.get()].query.rrTypes, vector<ResourceRecord> {}));
			intG[node.get()].answer = answer;
		}
	}
}

void BuildInterpretationGraph(EC& query, InterpreterGraphWrapper& intGraph_wrapper)
{
	NameServerIntpre nameServerNodesMap;
	//Add a dummy vertex as the start node over the top Name Servers
	intGraph_wrapper.startVertex = boost::add_vertex(intGraph_wrapper.intG);
	intGraph_wrapper.intG[intGraph_wrapper.startVertex].ns = "";
	intGraph_wrapper.intG[intGraph_wrapper.startVertex].query = query;
	nameServerNodesMap.insert(std::pair<string, std::vector<IntpVD>>("", std::vector<IntpVD>()));
	auto it = nameServerNodesMap.find("");
	if (it != nameServerNodesMap.end()) {
		it->second.push_back(intGraph_wrapper.startVertex);
	}
	else {
		Logger->critical(fmt::format("interpreter.cpp (BuildInterpretationGraph) - Unable to insert node into a interpretation graph"));
		std::exit(EXIT_FAILURE);
	}
	StartFromTop(intGraph_wrapper.intG, intGraph_wrapper.startVertex, query, nameServerNodesMap);
}
