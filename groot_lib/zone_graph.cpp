#include "zone.h"
#include "graph.h"

std::map<string, std::vector<int>> gNameServerZoneMap;
std::vector<string> gTopNameServers;
std::map<int, Zone> gZoneIdToZoneMap;

LabelMap ConstructLabelMap(const ZoneGraph& g, VertexDescriptor node) {
	LabelMap m;
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(node, g))) {
			m[g[edge.m_target].name] = edge.m_target;
	}
	return m;
}

ZoneVertexDescriptor GetAncestor(const ZoneGraph& g, const ZoneVertexDescriptor root, const vector<Label>& labels, std::map<VertexDescriptor, LabelMap>& domainChildLabelMap, int& index) {

	ZoneVertexDescriptor closestEncloser = root;
	if (labels.size() == index) {
		return closestEncloser;
	}
	if (out_degree(closestEncloser, g) > kHashMapThreshold) {
		if (domainChildLabelMap.find(closestEncloser) == domainChildLabelMap.end()) {
			domainChildLabelMap[closestEncloser] = ConstructLabelMap(g, closestEncloser);
		}
		LabelMap& m = domainChildLabelMap.find(closestEncloser)->second;
		auto it = m.find(labels[index]);
		if (it != m.end()) {
			closestEncloser = it->second;
			index++;
			return GetAncestor(g, closestEncloser, labels, domainChildLabelMap, index);
		}
	}
	else {
		for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closestEncloser, g))) {
			if (g[v].name == labels[index]) {
				closestEncloser = v;
				index++;
				return GetAncestor(g, closestEncloser, labels, domainChildLabelMap, index);
			}
		}
	}
	return closestEncloser;
}

std::bitset<RRType::N> GetNodeRRTypes(vector<ResourceRecord>& rrs) {
	std::bitset<RRType::N> rrTypes;
	for (auto& record : rrs) {
		rrTypes[record.get_type()] = 1;
	}
	return rrTypes;
}

ZoneVertexDescriptor GetClosestEncloser(ZoneGraph& g, ZoneVertexDescriptor root, vector<Label>& labels, int& index) {

	ZoneVertexDescriptor closetEncloser = root;
	if (labels.size() == index) {
		return closetEncloser;
	}
	for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closetEncloser, g))) {
		if (g[v].name == labels[index]) {
			std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes(g[v].rrs);
			//If at any node, we encoutner NS records and they are not part of authoritative data then the search stops here
			// as they mark cuts along the bottom of a zone.
			if (nodeRRtypes[RRType::NS] == 1 and nodeRRtypes[RRType::SOA] != 1) {
				index++;
				return v;
			}
			closetEncloser = v;
			index++;
			return GetClosestEncloser(g, closetEncloser, labels, index);
		}
	}
	return closetEncloser;
}

ZoneVertexDescriptor AddNodes(ZoneGraph& g, ZoneVertexDescriptor closetEncloser, vector<Label>& labels, std::map<VertexDescriptor, LabelMap>& domainChildLabelMap, int& index) {

	for (int i = index; i < labels.size(); i++) {
		ZoneVertexDescriptor u = boost::add_vertex(g);
		g[u].name = labels[i];
		ZoneEdgeDescriptor e; bool b;
		boost::tie(e, b) = boost::add_edge(closetEncloser, u, g);
		if (!b) {
			cout << "Unable to add edge" << endl;
			exit(EXIT_FAILURE);
		}
		//Only the first closestEncloser might have a map. For other cases closestEncloser will take care.
		if (domainChildLabelMap.find(closetEncloser) != domainChildLabelMap.end()) {
			LabelMap& m = domainChildLabelMap.find(closetEncloser)->second;
			m[labels[i]] = u;
		}
		closetEncloser = u;
	}
	return closetEncloser;
}

ZoneVertexDescriptor ZoneGraphBuilder(ResourceRecord& record, Zone& z) {
	vector<Label> labels = record.get_name();
	int index = 0;
	ZoneVertexDescriptor closetEncloser = GetAncestor(z.g, z.startVertex, labels, z.domainChildLabelMap, index);
	ZoneVertexDescriptor node = AddNodes(z.g, closetEncloser, labels, z.domainChildLabelMap, index);
	z.g[node].rrs.push_back(record);
	return node;
}

[[deprecated("Use ZoneGrapBuilder on a single RR to facilitate return of zoneVertex id")]]
void ZoneGraphBuilder(vector<ResourceRecord>& rrs, Zone& z) {
	
	for (auto& record : rrs)
	{
		if (record.get_type() != RRType::N) {
			ZoneGraphBuilder(record, z);
		}
		if (record.get_type() == RRType::SOA) {
			z.origin = record.get_name();
		}
	}
}

void BuildZoneLabelGraphs(string filePath, string nameServer, LabelGraph& g, const VertexDescriptor& root) {

	int zoneId = 1;
	if (gZoneIdToZoneMap.begin() != gZoneIdToZoneMap.end()) {
		zoneId = gZoneIdToZoneMap.rbegin()->first + 1;
	}
	Zone zone;
	zone.zoneId = zoneId;
	ZoneVertexDescriptor start = boost::add_vertex(zone.g);
	zone.g[start].name.set(".");
	zone.startVertex = start;
	ParseZoneFile(filePath, g, root, zone);
	string zoneName = "";
	for (auto label : zone.origin) {
		zoneName += label.get() + ".";
	}
	zoneName += "--" + nameServer + ".txt";
	//serialize(zoneGraph, "..\\tests\\SerializedGraphs\\" + zoneName);
	gZoneIdToZoneMap.insert(std::pair<int, Zone>(zoneId, zone));
	auto it = gNameServerZoneMap.find(nameServer);
	if (it == gNameServerZoneMap.end()) {
		gNameServerZoneMap.insert(std::pair<string, vector<int>>(nameServer, std::vector<int>{}));
	}
	it = gNameServerZoneMap.find(nameServer);
	if (it == gNameServerZoneMap.end()) {
		cout << "Unable to insert into map" << endl;
		std::exit(EXIT_FAILURE);
	}
	else {
		it->second.push_back(zoneId);
	}
	/*cout << "Number of nodes in Label Graph:" << num_vertices(g)<<endl<<flush;
	cout << "Number of nodes in Zone Graph:" << num_vertices(zone.g) << endl << flush;*/
}

vector<ResourceRecord> NSRecordLookUp(ZoneGraph& g, ZoneVertexDescriptor root, vector<ResourceRecord>& NSRecords, std::map<VertexDescriptor, LabelMap>& domainChildLabelMap) {
	vector<ResourceRecord> IPrecords;
	for (auto& record : NSRecords) {
		vector<Label> nsName = GetLabels(record.get_rdata());
		int index = 0;
		ZoneVertexDescriptor closetEncloser = GetAncestor(g, root, nsName,domainChildLabelMap, index);
		if (nsName.size() == index) {
			//found the node
			for (auto& noderecords : g[closetEncloser].rrs) {\
				if (noderecords.get_type() == RRType::A || noderecords.get_type() == RRType::AAAA) {
					IPrecords.push_back(noderecords);
				}
			}
		}	
	}
	return IPrecords;
}


boost::optional<vector<ResourceRecord>> QueryLookUp(Zone& z, EC& query, bool& completeMatch) {
	// For No matching case(query doesn't belong to this zone file) function returns {}
	// For matching but with empty record functions return an empty vector - NXDOMAIN
	// Otherwise a vector of RR's
	int index = 0;
	for (Label l : z.origin) {
		if (index>=query.name.size() || l.n != query.name[index].n) {
			return {};
		}
		index++;
	}
	index = 0;
	ZoneVertexDescriptor closetEncloser = GetClosestEncloser(z.g, z.startVertex, query.name, index);
	vector<ResourceRecord> answer;

	if (query.name.size() != index) {
	
		//WildCard Child case
		for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closetEncloser, z.g))) {
			if (z.g[v].name.get() == "*") {
				completeMatch = true;
				for (auto& record : z.g[v].rrs) {
					if (record.get_type() == RRType::CNAME) {
						answer.push_back(record);
						return boost::make_optional(answer); //If CNAME then there will be no other record
					}
					if (query.rrTypes[record.get_type()] == 1) {
						answer.push_back(record);
					}
					//NS records at a wildcard node are forbidden.
				}
				return boost::make_optional(answer);
			}
		}

		completeMatch = false;
		vector<ResourceRecord> NSRecords;
		for (auto& record : z.g[closetEncloser].rrs) {
			if (record.get_type() == RRType::DNAME ) {
				//DNAME is a singleton type, there can be other records at this node but since we have labels left to match we return
				answer.push_back(record);
				return boost::make_optional(answer);
			}
			if (record.get_type() == RRType::NS) {
				NSRecords.push_back(record);
			}
		}
		answer.insert(answer.end(), NSRecords.begin(), NSRecords.end());
		vector<ResourceRecord> IPrecords = NSRecordLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
		answer.insert(answer.end(), IPrecords.begin(), IPrecords.end());
		return boost::make_optional(answer);
	}
	else {
		completeMatch = true;
		if (query.excluded) {
			vector<ResourceRecord> NSRecords;
			boost::optional<ResourceRecord> SOA;
			for (auto& record : z.g[closetEncloser].rrs) {
				if (record.get_type() == RRType::NS) {
					NSRecords.push_back(record);
				}
				if (record.get_type() == RRType::SOA) {
					SOA = boost::make_optional(record);
				}
			}
			//If the node is a zone cut then return the NS records along with glue records
			if (!SOA) {
				if (NSRecords.size() > 0) {
					answer.insert(answer.end(), NSRecords.begin(), NSRecords.end());
					vector<ResourceRecord> IPrecords = NSRecordLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
					answer.insert(answer.end(), IPrecords.begin(), IPrecords.end());
				}
			}
			if(SOA || (!SOA && NSRecords.size() == 0)) {
				//If there is excluded and the node has a wildcard then return records matching the query type
				//If there is no wildcard child then its a NXDOMAIN Case
				//If there is wildcard and non matching types then also its a NXDOMAIN case
				for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closetEncloser, z.g))) {
					if (z.g[v].name.get() == "*") {
						for (auto& record : z.g[v].rrs) {
							if (record.get_type() == RRType::CNAME) {
								answer.clear();
								answer.push_back(record);
								return boost::make_optional(answer); //If CNAME then there will be no other record
							}
							if (query.rrTypes[record.get_type()] == 1) {
								answer.push_back(record);
							}
							//NS records at a wildcard node are forbidden.
						}
					}
				}
			}			
			return boost::make_optional(answer);
		}
		else {
			//Exact Query Match
			vector<ResourceRecord> NSRecords;
			boost::optional<ResourceRecord> SOA;
			for (auto& record : z.g[closetEncloser].rrs) {
				if (record.get_type() == RRType::CNAME) {
					answer.clear();
					answer.push_back(record);
					return boost::make_optional(answer); //If CNAME then there will be no other record
				}
				if (query.rrTypes[record.get_type()] == 1) {
					answer.push_back(record);
				}
				if (record.get_type() == RRType::NS) {
					NSRecords.push_back(record);
				}
				// SOA record and NS records are used  to distinguish whether the answer is authoritative or not.
				// If SOA record is absent and NS records are present and a full match happens then the node will have only NS records(BIND ignores other records even if they are present).
				// If SOA record and NS records are present then we are the top of zone and we will return SOA (authoritative)
				// If SOA record and NS records are absent then the query types not found are NXDOMAIN since this Name Server is an (authoritative) for query name.
				if (record.get_type() == RRType::SOA) {
					SOA = boost::make_optional(record);
				}
			}
			if (!SOA) {
				//There is no SOA record at the node
				if (NSRecords.size() > 0) {
					answer.clear();
					//This node is a zone cut node and should have only NS records. Return the NS records along with glue records
					answer.insert(answer.end(), NSRecords.begin(), NSRecords.end());
					vector<ResourceRecord> IPrecords = NSRecordLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
					answer.insert(answer.end(), IPrecords.begin(), IPrecords.end());
				}
				else {
					//Zone is authoritative of this node and types not found will be NXDOMAIN.
				}
			}
			else if(query.rrTypes[RRType::SOA] != 1) {
				//If the query did not ask for SOA record then send it as an indication that no further queries has to be made using the name servers in the answer.
				answer.push_back(SOA.get());
			}
			if (query.rrTypes[RRType::NS] == 1) {
				//If the query asked for NS records then add glue records
				vector<ResourceRecord> IPrecords = NSRecordLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
				answer.insert(answer.end(), IPrecords.begin(), IPrecords.end());
			}
			return boost::make_optional(answer);
		}
	}
}
