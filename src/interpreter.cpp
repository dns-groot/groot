#include "interpreter.h"

std::map<string, std::vector<Zone>> gNameServerZoneMap;
std::vector<string> gTopNameServers;


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
		string label = "[label=\"NS: " + nameServer + queryName + LabelsToString(query.name) + " T:" + RRTypesToString(query.rrTypes) + "  \\n A:";
		std::bitset<RRType::N> answerTypes;
		if (answer) {
			for (auto r : answer.get()) {
				answerTypes[r.get_type()] = 1;
			}
		}
		label += RRTypesToString(answerTypes) + "\"]";
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
	return node_writer<NSMap, QueryMap, AnswerMap>(ns, q,a);
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
			cout << "Error in DNAME processing" << endl;
			exit(EXIT_FAILURE);
		}
		i++;
	}
	vector<Label> rdataLabels = GetLabels(record.get_rdata());
	vector<Label> namelabels = record.get_name();
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
	newQuery.name = GetLabels(record.get_rdata());
	return newQuery;
}

boost::optional<Zone> GetRelevantZone(string ns, EC& query) {
	auto it = gNameServerZoneMap.find(ns);
	auto queryLabels = query.name;
	if (it != gNameServerZoneMap.end()) {
		int max = 0;
		Zone bestMatch = it->second[0];
		for (auto z : it->second) {
			int i = 0;
			for (auto l : z.origin) {
				if (i >= queryLabels.size() || !(l == queryLabels[i])) {
					break;
				}
				i++;
			}
			if (i > max) {
				max = i;
				bestMatch = z;
			}
		}
		return boost::make_optional(bestMatch);
	}
	else {
		return {};
	}
}

InterpreterVertexDescriptor SideQuery(std::map<string, std::vector<InterpreterVertexDescriptor>>& nameServer_nodes_map, InterpreterGraph& intG, EC& query) {
	auto it = nameServer_nodes_map.find("");
	if (it == nameServer_nodes_map.end()) {
		nameServer_nodes_map.insert(std::pair<string, std::vector<InterpreterVertexDescriptor>>("", std::vector<InterpreterVertexDescriptor>()));
	}
	else {
		std::vector<InterpreterVertexDescriptor> existingNodes = it->second;
		for (auto n : existingNodes) {
			if (CheckQueryEquivalence(query, intG[n].query)) {
				return n;
			}
		}
	}
	//Add a dummy vertex as the start node over the top Name Servers
	InterpreterVertexDescriptor dummy = boost::add_vertex(intG);
	intG[dummy].ns = "";
	intG[dummy].query = query;
	it = nameServer_nodes_map.find("");
	if (it != nameServer_nodes_map.end()) {
		it->second.push_back(dummy);
	}
	else {
		cout << "Unable to insert into map" << endl;
		std::exit(EXIT_FAILURE);
	}
	StartFromTop(intG, dummy, query, nameServer_nodes_map);
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

void NameServer(Zone& z, InterpreterGraph& g, InterpreterVertexDescriptor& v, std::map<string, std::vector<InterpreterVertexDescriptor>>& nameServerZoneMap) {

	bool completeMatch = false;
	g[v].answer = QueryLookUp(z, g[v].query, completeMatch);
	if (g[v].answer) {
		vector<ResourceRecord> answer = g[v].answer.get();
		if (!completeMatch) {
			if (answer.size() > 0) {
				//DNAME takes preference over NS as it is not a complete match
				bool foundDNAME = false;
				for (auto& record : answer) {
					if (record.get_type() == RRType::DNAME) {
						foundDNAME = true;
						//process DNAME Record and get new query
						EC newQuery = ProcessDNAME(record, g[v].query);				

						//Search for relevant zone at the same name server first
						boost::optional<Zone> start = GetRelevantZone(g[v].ns, newQuery);
						if (start) {
							boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServerZoneMap, g, g[v].ns, newQuery, v, {});
							//If there was no query to this NS then continue the querying
							if (node) {
								NameServer(start.get(), g, node.get(), nameServerZoneMap);
							}
						}
						else {
							//Start from top Name Servers
							StartFromTop(g, v, newQuery, nameServerZoneMap);
						}
						break;
					}
				}
				if (!foundDNAME) {
					//Then the records should be of type NS and may be their IP records
					for (auto& record : answer) {
						if (record.get_type() == RRType::NS) {
							bool foundIP = false;
							string newNS = record.get_rdata();
							for (auto& r : answer) {
								if (r.get_name() == GetLabels(record.get_rdata())) {
									foundIP = true;
								}
							}
							//Either the IP address has to be found or the referral to a topNameServer
							if (foundIP|| (std::find(gTopNameServers.begin(), gTopNameServers.end(),newNS) != gTopNameServers.end())) {
								boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServerZoneMap, g, newNS, g[v].query, v, {});
								if (node) {
									boost::optional<Zone> start = GetRelevantZone(newNS, g[v].query);
									if (start) {
										NameServer(start.get(), g, node.get(), nameServerZoneMap);
									}
									else {
										//Path terminates
									}
								}
							}
							else {
								// Have to query for the IP address of NS
								EC nsQuery;
								nsQuery.name = GetLabels(newNS);
								nsQuery.rrTypes[RRType::A] = 1;
								nsQuery.rrTypes[RRType::AAAA] = 1;
								InterpreterVertexDescriptor nsStart = SideQuery(nameServerZoneMap, g, nsQuery);
								//TODO: Check starting from this nsStart node if we got the ip records
								boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServerZoneMap, g, newNS, g[v].query, v, nsStart);
								if (node) {
									boost::optional<Zone> start = GetRelevantZone(newNS, g[v].query);
									if (start) {
										NameServer(start.get(), g, node.get(), nameServerZoneMap);
									}
									else {
										//Path terminates
									}
								}
							}
						}
					}
				}
			}
			else {
				// No records found - NXDOMAIN
				// The current path terminates
			}
		}
		else {
			//Complete match case
			if (answer.size() > 0) {
				if (answer[0].get_type() == RRType::CNAME) {
					//Guaranteed to be the only record
					//process CNAME Record and get new query
					EC newQuery = ProcessCNAME(answer[0], g[v].query);
					if (newQuery.rrTypes.count() > 0) {
						//Search for relevant zone at the same name server first
						boost::optional<Zone> start = GetRelevantZone(g[v].ns, newQuery);
						if (start) {
							boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServerZoneMap, g, g[v].ns, newQuery, v, {});
							//If there was no query to this NS then continue the querying
							if (node) {
								NameServer(start.get(), g, node.get(), nameServerZoneMap);
							}
						}
						else {
							//Start from the top Zone file 
							StartFromTop(g, v, newQuery, nameServerZoneMap);
						}
					}
				}
				else {
					boost::optional<ResourceRecord> SOA;
					bool foundNS = false;
					for (auto& record : answer) {
						if (record.get_type() == RRType::SOA) {
							SOA = boost::make_optional(record);
						}
						if (record.get_type() == RRType::NS) {
							foundNS = true;
						}
					}
					if (!SOA) {
						if (foundNS) {
							//Leaf node of that zone and is a cut. Only NS records should be in the answer
							vector<ResourceRecord> glueRecords = SeparateGlueRecords(answer);
							for (auto& record : answer) {
								if (record.get_type() == RRType::NS) {
									bool foundIP = false;
									string newNS = record.get_rdata();
									for (auto& glueRR : glueRecords) {
										if (glueRR.get_name() == GetLabels(record.get_rdata())) {
											foundIP = true;
										}
									}								
									if (foundIP || (std::find(gTopNameServers.begin(), gTopNameServers.end(), newNS) != gTopNameServers.end())) {
										boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServerZoneMap, g, newNS, g[v].query, v, {});
										if (node) {
											boost::optional<Zone> start = GetRelevantZone(newNS, g[v].query);
											if (start) {
												NameServer(start.get(), g, node.get(), nameServerZoneMap);
											}
											else {
												//Path terminates
											}
										}
									}
									else {
										// Have to query for the IP address of NS
										EC nsQuery;
										nsQuery.name = GetLabels(newNS);
										nsQuery.rrTypes[RRType::A] = 1;
										nsQuery.rrTypes[RRType::AAAA] = 1;
										InterpreterVertexDescriptor nsStart = SideQuery(nameServerZoneMap, g, nsQuery);
										//TODO: Check starting from this nsStart node if we got the ip records and proceed (but insert node for delegation checking)
										boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServerZoneMap, g, newNS, g[v].query, v, nsStart);
										if (node) {
											boost::optional<Zone> start = GetRelevantZone(newNS, g[v].query);
											if (start) {
												NameServer(start.get(), g, node.get(), nameServerZoneMap);
											}
											else {
												//Path terminates
											}
										}
									}

								}
							}
						}
						else {
							//Answer was authoritative and types not found are NXDOMAIN
						}
					}
				}
			}
			else {
				// No records found - NXDOMAIN
				// The current path terminates
			}
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

boost::optional<InterpreterVertexDescriptor> InsertNode(std::map<string, std::vector<InterpreterVertexDescriptor>>& nameServer_nodes_map, InterpreterGraph& intG, string ns, EC query, InterpreterVertexDescriptor edgeStart, boost::optional<InterpreterVertexDescriptor> edgeQuery) {
	//First checks if a node exists in the graph with NS = ns and Query = query
	//If it exists, then it add an edge from the edgeStart to found node and returns {}
	//Else Creates a new node and adds an edge and returns the new node
	auto it = nameServer_nodes_map.find(ns);
	if (it == nameServer_nodes_map.end()) {
		nameServer_nodes_map.insert(std::pair<string, std::vector<InterpreterVertexDescriptor>>(ns, std::vector<InterpreterVertexDescriptor>()));
	}
	else {
		std::vector<InterpreterVertexDescriptor> existingNodes = it->second;
		for (auto n : existingNodes) {
			if (CheckQueryEquivalence(query, intG[n].query)) {
				InterpreterEdgeDescriptor e; bool b;
				if (edgeStart != n) {
					boost::tie(e, b) = boost::add_edge(edgeStart, n, intG);
					if (!b) {
						cout << "Unable to add edge" << endl;
						std::exit(EXIT_FAILURE);
					}
					if (edgeQuery) {
						intG[e].intermediateQuery = boost::make_optional(edgeQuery.get());
					}
				}
				return {};
			}
		}
	}
	it = nameServer_nodes_map.find(ns);
	if (it != nameServer_nodes_map.end()) {
		InterpreterVertexDescriptor v = boost::add_vertex(intG);
		
		intG[v].ns = ns;
		intG[v].query = query;
		InterpreterEdgeDescriptor e; bool b;
		boost::tie(e, b) = boost::add_edge(edgeStart, v, intG);
		if (!b) {
			cout << "Unable to add edge" << endl;
			std::exit(EXIT_FAILURE);
		}
		if (edgeQuery) {
			intG[e].intermediateQuery = boost::make_optional(edgeQuery.get());
		}
		it->second.push_back(v);
		return v;
	}
	else {
		cout << "Unable to insert into map" << endl;
		std::exit(EXIT_FAILURE);
	}

}

void StartFromTop(InterpreterGraph& intG, InterpreterVertexDescriptor edgeStartNode, EC& query, std::map<string, std::vector<InterpreterVertexDescriptor>>& nameServer_nodes_map) {
	for (string ns : gTopNameServers) {
		boost::optional<Zone> start = GetRelevantZone(ns, query);
		if (start) {
			boost::optional<InterpreterVertexDescriptor> node = InsertNode(nameServer_nodes_map, intG, ns, query, edgeStartNode, {});
			if (node) {
				NameServer(start.get(), intG, node.get(), nameServer_nodes_map);
			}
		}
	}
}

void BuildInterpretationGraph(EC& query, InterpreterGraphWrapper& intGraph_wrapper)
{	
	std::map<string, std::vector<InterpreterVertexDescriptor>> nameServer_nodes_map;
	//Add a dummy vertex as the start node over the top Name Servers
	intGraph_wrapper.startVertex = boost::add_vertex(intGraph_wrapper.intG);
	intGraph_wrapper.intG[intGraph_wrapper.startVertex].ns = "";
	intGraph_wrapper.intG[intGraph_wrapper.startVertex].query = query;
	nameServer_nodes_map.insert(std::pair<string, std::vector<InterpreterVertexDescriptor>>("", std::vector<InterpreterVertexDescriptor>()));
	auto it = nameServer_nodes_map.find("");
	if (it != nameServer_nodes_map.end()) {
		it->second.push_back(intGraph_wrapper.startVertex);
	}
	else {
		cout << "Unable to insert into map" << endl;
		std::exit(EXIT_FAILURE);
	}
	StartFromTop(intGraph_wrapper.intG, intGraph_wrapper.startVertex, query, nameServer_nodes_map);
}
