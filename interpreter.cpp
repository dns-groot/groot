#include "interpreter.h"



EC process_DNAME(ResourceRecord& record, EC& query) {
	auto pos = query.name.find_last_of(record.GetName());
	EC newQuery;
	newQuery.excluded = query.excluded;
	newQuery.rrTypes = query.rrTypes;
	newQuery.name = query.name.substr(0, pos) + record.GetRData();
	return newQuery;
}

EC process_CNAME(ResourceRecord& record, EC& query) {
	EC newQuery;
	newQuery.rrTypes = query.rrTypes;
	newQuery.rrTypes.reset(rr_type::CNAME);
	newQuery.name = record.GetRData();
	return newQuery;
}

boost::optional<zone> getRelevantZone(string ns, EC& query) {
	auto it = nameServer_Zone_Map.find(ns);
	auto queryLabels = getLabels(query.name);
	if (it != nameServer_Zone_Map.end()) {
		int max = 0;
		zone bestMatch = it->second[0];
		for (auto z : it->second) {
			int i = 0;
			for (auto l : z.origin) {
				if (i >= queryLabels.size() || l != queryLabels[i]) {
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

void nameServer( zone& z, intGraph& g, intVertex_t& v) {

	bool completeMatch = false;
	g[v].answer = queryLookUp(z, g[v].query, completeMatch);
	if (g[v].answer) {
		vector<ResourceRecord> answer = g[v].answer.get();
		if (!completeMatch) {
			if (answer.size() > 0) {
				//DNAME takes preference over NS as it is not a complete match
				// Having both records is possible only at the top in which case DNAME is executed.
				bool found_dname = false;
				for (auto& record : answer) {
					if (record.GetType() == rr_type::DNAME) {
						//process DNAME Record and rewrite the query
						found_dname = true;
						intVertex_t node = boost::add_vertex(g);
						g[node].query = process_DNAME(record, g[v].query);
						intEdge_t e; bool b;
						boost::tie(e, b) = boost::add_edge(v, node, g);
						if (!b) {
							cout << "Unable to add edge" << endl;
							exit(EXIT_FAILURE);
						}
						g[node].ns = g[v].ns; 
						//nameServer(getRelevantZone(g[v].ns, g[node].query), g, node);
						break;
					}
				}
				if (!found_dname) {
					//Then the records should be of type NS and may be their IP records
					for (auto& record : answer) {
						if (record.GetType() == rr_type::NS) {
							bool found_IP = false;
							for (auto& r : answer) {
								if (r.GetName() == record.GetRData()) {
									found_IP = true;
								}
							}
							if (found_IP) {
								intVertex_t node = boost::add_vertex(g);
								g[node].query = g[v].query;
								intEdge_t e; bool b;
								boost::tie(e, b) = boost::add_edge(v, node, g);
								if (!b) {
									cout << "Unable to add edge" << endl;
									exit(EXIT_FAILURE);
								}
								g[node].ns = record.GetRData();
								//nameServer(getRelevantZone(record.GetRData()), g, node);
							}
							else {
								// Have to query for the IP address of NS
							}
						}
					}
				}
			}
			else {
				// No records found - NXDOMAIN
				// Graph terminates
			}
		}
		else {
			//Complete match case
			if (answer.size() > 0) {
				if (answer[0].GetType() == rr_type::CNAME) {
					//Guaranteed to be the only record
					intVertex_t node = boost::add_vertex(g);
					g[node].query = process_CNAME(answer[0], g[v].query);
					intEdge_t e; bool b;
					boost::tie(e, b) = boost::add_edge(v, node, g);
					if (!b) {
						cout << "Unable to add edge" << endl;
						exit(EXIT_FAILURE);
					}
					g[node].ns = g[v].ns;
					//nameServer(getRelevantZone(g[v].ns, g[node].query), g, node);
				}
				else {
					std::bitset<rr_type::N> typesFound;
					for (auto& record : answer) {
						typesFound.set(record.GetType());
					}
					typesFound.flip();
					for (auto& record : answer) {
						if (record.GetType() == rr_type::NS) {
							bool found_IP = false;
							for (auto& r : answer) {
								if (r.GetName() == record.GetRData()) {
									found_IP = true;
								}
							}
							if (found_IP) {
								intVertex_t node = boost::add_vertex(g);
								g[node].query = g[v].query;
								g[node].query.rrTypes = g[node].query.rrTypes & typesFound;
								intEdge_t e; bool b;
								boost::tie(e, b) = boost::add_edge(v, node, g);
								if (!b) {
									cout << "Unable to add edge" << endl;
									exit(EXIT_FAILURE);
								}
								g[node].ns = record.GetRData();
								boost::optional<zone> start = getRelevantZone(record.GetRData(), g[node].query);
								if (start) {
									nameServer(start.get(), g, node);
								}								
							}
							else {
								// Have to query for the IP address of NS
							}
						}
					}

				}
			}
			else {
				// No records found - NXDOMAIN
				// Graph terminates
			}
		}
	}
	else {
		//Search for the next best server/top name server?
	}
}



void build_interpreter_graph(string ns, EC& query, intGraph& g)
{
	boost::optional<zone> start = getRelevantZone(ns, query);
	if (start) {
		intVertex_t root = boost::add_vertex(g);
		g[root].query = query;
		g[root].ns = ns;
		nameServer(start.get(), g, root);
	}	
}
