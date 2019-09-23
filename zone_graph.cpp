#include "zone.h"
#include "graph.h"

zoneVertex_t addNodes(zoneGraph& g, zoneVertex_t closetEncloser, vector<std::string>& labels, int& index) {

	for (int i = index; i < labels.size(); i++) {
		zoneVertex_t u = boost::add_vertex(g);
		g[u].name = labels[i];
		zoneEdge_t e; bool b;
		boost::tie(e, b) = boost::add_edge(closetEncloser, u, g);
		if (!b) {
			cout << "Unable to add edge" << endl;
			exit(EXIT_FAILURE);
		}
		closetEncloser = u;
	}
	return closetEncloser;
}

zoneVertex_t getClosestEncloser(zoneGraph& g, zoneVertex_t root, vector<std::string>& labels, int& index) {

	zoneVertex_t closetEncloser = root;
	if (labels.size() == index) {
		return closetEncloser;
	}
	for (zoneVertex_t v : boost::make_iterator_range(adjacent_vertices(closetEncloser, g))) {		
		if (g[v].name.compare(labels[index]) == 0) {
			closetEncloser = v;
			index++;
			return getClosestEncloser(g, closetEncloser, labels, index);
		}
	}
	return closetEncloser;
}

void zone_graph_builder(vector<ResourceRecord>& rrs, zone& z) {
	// TODO: Has to be changed to have glue records separate from the label tree.
	zoneVertex_t root = boost::add_vertex(z.g);
	z.g[root].name = ".";
	z.startVertex = root;

	for (auto& record : rrs)
	{
		if (record.GetType() != rr_type::N) {
			string name = record.GetName();
			vector<std::string> labels = getLabels(name);
			int index = 0;
			zoneVertex_t closetEncloser = getClosestEncloser(z.g, root, labels, index);
			zoneVertex_t node = addNodes(z.g, closetEncloser, labels, index);
			z.g[node].rrs.push_back(record);
		}
		if (record.GetType() == rr_type::SOA) {
			z.origin = std::move(getLabels(record.GetName()));
		}
	}
}

vector<ResourceRecord> NSRecordLookUp(zoneGraph& g, zoneVertex_t root, vector<ResourceRecord>& NSRecords) {
	vector<ResourceRecord> IPrecords;
	for (auto& record : NSRecords) {
		vector<std::string> nsName = getLabels(record.GetRData());
		int index = 0;
		zoneVertex_t closetEncloser = getClosestEncloser(g, root, nsName, index);
		if (nsName.size() == index) {
			//found the node
			for (auto& noderecords : g[closetEncloser].rrs) {\
				if (noderecords.GetType() == rr_type::A || noderecords.GetType() == rr_type::AAAA) {
					IPrecords.push_back(noderecords);
				}
			}
		}	
	}
	return IPrecords;
}


boost::optional<vector<ResourceRecord>> queryLookUp(zone& z, EC& query, bool& completeMatch) {
	// For No matching case(query doesn't belong to this zone file) function returns {}
	// For matching but with empty record functions return an empty vector - NXDOMAIN
	// Otherwise a vector of RR's
	vector<std::string> labels = getLabels(query.name);
	int index = 0;
	for (string s : z.origin) {
		if (s != labels[index]) {
			return {};
		}
		index++;
	}
	index = 0;
	zoneVertex_t closetEncloser = getClosestEncloser(z.g, z.startVertex, labels, index);
	vector<ResourceRecord> answer;

	if (labels.size() != index) {
		// WildCard child case is not possible
		for (auto& record : z.g[closetEncloser].rrs) {
			if (record.GetType() == rr_type::DNAME || record.GetType() == rr_type::NS) {
				answer.push_back(record);
			}
		}
		return boost::make_optional(answer);
	}
	else {
		completeMatch = true;
		if (query.excluded) {
			//If there is excluded and the node has a wildcard then return records matching the query type
			//If there is no wildcard child then its a NXDOMAIN Case
			//If there is wildcard and non matching types then also its a NXDOMAIN case
			for (zoneVertex_t v : boost::make_iterator_range(adjacent_vertices(closetEncloser, z.g))) {
				if (z.g[v].name.compare("*") == 0) {
					vector<ResourceRecord> NSRecords;
					for (auto& record : z.g[v].rrs) {
						if (query.rrTypes[record.GetType()] == 1) {
							answer.push_back(record);
						}
						if (record.GetType() == rr_type::CNAME){
							answer.push_back(record);
							return boost::make_optional(answer); //If CNAME then there will be no other record
						}
						//NS records at a wildcard node are forbidden.
					}
				}
			}
			return boost::make_optional(answer);
		}
		else {
			//Exact Query Match
			std::bitset<rr_type::N> typesFound;
			vector<ResourceRecord> NSRecords;
			for (auto& record : z.g[closetEncloser].rrs) {
				if (query.rrTypes[record.GetType()] == 1) {
					typesFound[record.GetType()] = 1;
					answer.push_back(record);
				}
				if (record.GetType() == rr_type::CNAME) {
					answer.push_back(record);
					return boost::make_optional(answer); //If CNAME then there will be no other record
				}
				if (record.GetType() == rr_type::NS) {
					NSRecords.push_back(record);
				}
			}
			if (typesFound != query.rrTypes) {
				if (query.rrTypes[rr_type::NS] != 1) {
					answer.insert(answer.end(), NSRecords.begin(), NSRecords.end());
				}
				vector<ResourceRecord> IPrecords = NSRecordLookUp(z.g, z.startVertex, NSRecords);
				answer.insert(answer.end(), IPrecords.begin(), IPrecords.end());
			}
			return boost::make_optional(answer);
		}
	}
}