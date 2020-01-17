#include "zone.h"
#include "graph.h"

std::map<string, std::vector<int>> gNameServerZoneMap;
std::vector<string> gTopNameServers;
std::map<int, Zone> gZoneIdToZoneMap;

string ReturnTagToString(ReturnTag& r) {
	if (r == ReturnTag::ANS)
		return "ANS";
	if (r == ReturnTag::ANSQ)
		return "ANSQ";
	if (r == ReturnTag::REF)
		return "REF";
	if (r == ReturnTag::NX)
		return "NX";
	if (r == ReturnTag::REFUSED)
		return "REFUSED";
	return "";
}

string SearchForNameServer(int zoneId) {
	/* Given a zoneId return the name server which hosts thay zone.*/
	for (auto const& [ns, zoneIds] : gNameServerZoneMap) {
		if (std::find(zoneIds.begin(), zoneIds.end(), zoneId) != zoneIds.end()) {
			return ns;
		}
	}
	cout << "ZoneId not found in the Name Server ZoneIds map" << endl;
	exit(EXIT_FAILURE);
}

LabelMap ConstructLabelMap(const ZoneGraph& g, VertexDescriptor node) {
	LabelMap m;
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(node, g))) {
		m[g[edge.m_target].name] = edge.m_target;
	}
	return m;
}

ZoneVertexDescriptor GetAncestor(const ZoneGraph& g, const ZoneVertexDescriptor root, const vector<Label>& labels, std::map<VertexDescriptor, LabelMap>& domainChildLabelMap, int& index) {
	/*Given a domain this function returns its closest ancestor in the existing Zone Graph. This function is used for building the Zone Graph. */
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

ZoneVertexDescriptor GetClosestEncloser(Zone& z, ZoneVertexDescriptor root, vector<Label>& labels, int& index) {

	ZoneVertexDescriptor closestEncloser = root;
	if (labels.size() == index) {
		return closestEncloser;
	}
	if (z.domainChildLabelMap.find(closestEncloser) != z.domainChildLabelMap.end()) {
		LabelMap& m = z.domainChildLabelMap.find(closestEncloser)->second;
		auto it = m.find(labels[index]);
		if (it != m.end()) {
			closestEncloser = it->second;
			index++;
			std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes(z.g[closestEncloser].rrs);
			//If at any node, we encoutner NS records and they are not part of authoritative data then the search stops here
			// as they mark cuts along the bottom of a zone.
			if (nodeRRtypes[RRType::NS] == 1 and nodeRRtypes[RRType::SOA] != 1) {
				return closestEncloser;
			}
			return GetClosestEncloser(z, closestEncloser, labels, index);
		}
	}
	else {
		for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closestEncloser, z.g))) {
			if (z.g[v].name == labels[index]) {
				std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes(z.g[v].rrs);
				index++;
				if (nodeRRtypes[RRType::NS] == 1 and nodeRRtypes[RRType::SOA] != 1) {
					return v;
				}
				closestEncloser = v;
				return GetClosestEncloser(z, closestEncloser, labels, index);
			}
		}
	}
	return closestEncloser;
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
	if (record.get_type() == RRType::SOA) {
		z.origin = record.get_name();
	}
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

vector<ResourceRecord> GlueRecordsLookUp(ZoneGraph& g, ZoneVertexDescriptor root, vector<ResourceRecord>& NSRecords, std::map<VertexDescriptor, LabelMap>& domainChildLabelMap) {
	vector<ResourceRecord> IPrecords;
	for (auto& record : NSRecords) {
		vector<Label> nsName = GetLabels(record.get_rdata());
		int index = 0;
		ZoneVertexDescriptor closetEncloser = GetAncestor(g, root, nsName, domainChildLabelMap, index);
		if (nsName.size() == index) {
			//found the node
			for (auto& noderecords : g[closetEncloser].rrs) {
				if (noderecords.get_type() == RRType::A || noderecords.get_type() == RRType::AAAA) {
					IPrecords.push_back(noderecords);
				}
			}
		}
	}
	return IPrecords;
}

bool RequireGlueRecords(Zone z, vector<ResourceRecord>& NSRecords) {

	for (auto& record : NSRecords) {
		vector<Label> nsName = GetLabels(record.get_rdata());
		if (nsName.size() < z.origin.size()) continue;
		int i = 0;
		for (; i < z.origin.size(); i++) {
			if (z.origin[i].get() != nsName[i].get()) {
				break;
			}
		}
		if (i == z.origin.size()) return true;
	}
	return false;
}

boost::optional<vector<ZoneLookUpAnswer>> QueryLookUpAtZone(Zone& z, EC& query, bool& completeMatch) {
	//Query lookup at the zone is peformed only if it is relevant

	/*int index = 0;
	for (Label l : z.origin) {
		if (index >= query.name.size() || l.n != query.name[index].n) {
			return {};
		}
		index++;
	}*/
	int index = 0;
	ZoneVertexDescriptor closestEncloser = GetClosestEncloser(z, z.startVertex, query.name, index);
	vector<ZoneLookUpAnswer> answers;

	if (query.name.size() != index) {
		std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes(z.g[closestEncloser].rrs);
		//WildCard Child case - dq ∈∗ dr
		for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closestEncloser, z.g))) {
			if (z.g[v].name.get() == "*") {
				completeMatch = true;
				vector<ResourceRecord> matchingRRs;
				std::bitset<RRType::N> queryTypesFound;
				for (auto& record : z.g[v].rrs) {
					if (record.get_type() == RRType::CNAME) {
						answers.clear();
						matchingRRs.clear();
						matchingRRs.push_back(record);
						//Return type is Ans if only CNAME is requested
						if (query.rrTypes[RRType::CNAME] && query.rrTypes.count() == 1) {
							answers.push_back(std::make_tuple(ReturnTag::ANS, nodeRRtypes, matchingRRs));
						}
						else {
							answers.push_back(std::make_tuple(ReturnTag::ANSQ, nodeRRtypes, matchingRRs));
						}
						return boost::make_optional(answers); //If CNAME then other records would be ignored.
					}
					if (query.rrTypes[record.get_type()] == 1) {
						matchingRRs.push_back(record);
						queryTypesFound.set(record.get_type());
					}
					//NS records at a wildcard node are forbidden.
				}
				if (queryTypesFound.count())answers.push_back(std::make_tuple(ReturnTag::ANS, queryTypesFound, matchingRRs));
				if ((queryTypesFound ^ query.rrTypes).count()) {
					answers.push_back(std::make_tuple(ReturnTag::NX, queryTypesFound ^ query.rrTypes, vector<ResourceRecord> {}));
				}
				return boost::make_optional(answers);
			}
		}
		completeMatch = false;
		vector<ResourceRecord> NSRecords;
		
		for (auto& record : z.g[closestEncloser].rrs) {
			if (record.get_type() == RRType::DNAME) {
				// dr < dq ∧ DNAME ∈ T,  DNAME is a singleton type, there can be no other records of DNAME type at this node.
				vector<ResourceRecord> dname;
				dname.push_back(record);
				answers.push_back(std::make_tuple(ReturnTag::ANSQ, nodeRRtypes, dname));
				return boost::make_optional(answers);
			}
			if (record.get_type() == RRType::NS) {
				NSRecords.push_back(record);
			}
		}
		//dr < dq ∧ NS ∈ T ∧ SOA ~∈ T
		if (NSRecords.size() && !nodeRRtypes[RRType::SOA]) {
			vector<ResourceRecord> IPrecords = GlueRecordsLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
			NSRecords.insert(NSRecords.end(), IPrecords.begin(), IPrecords.end());
			answers.push_back(std::make_tuple(ReturnTag::REF, nodeRRtypes, NSRecords));
		}
		else {
			answers.push_back(std::make_tuple(ReturnTag::NX, query.rrTypes, vector<ResourceRecord> {}));
		}
		return boost::make_optional(answers);
	}
	else {
		completeMatch = true;
		if (query.excluded) {

			std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes(z.g[closestEncloser].rrs);
			vector<ResourceRecord> NSRecords;
			for (auto& record : z.g[closestEncloser].rrs) {
				if (record.get_type() == RRType::NS) {
					NSRecords.push_back(record);
				}
			}
			// If there are NS records, then get their glue records too.
			if (nodeRRtypes[RRType::NS]) {
				vector<ResourceRecord> IPrecords = GlueRecordsLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
				NSRecords.insert(NSRecords.end(), IPrecords.begin(), IPrecords.end());
			}
			// Referral case (Zone-Cut)
			if (nodeRRtypes[RRType::NS] && !nodeRRtypes[RRType::SOA]) {
				answers.push_back(std::make_tuple(ReturnTag::REF, nodeRRtypes, NSRecords));
				return boost::make_optional(answers);
			}
			//If there is query excluded and the node has a wildcard then return records matching the query type
			//If there is no wildcard child then its a NXDOMAIN Case
			//If there is wildcard and non matching types then its a Ans with empty records

			for (ZoneVertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closestEncloser, z.g))) {
				if (z.g[v].name.get() == "*") {
					vector<ResourceRecord> matchingRRs;
					std::bitset<RRType::N> queryTypesFound;
					for (auto& record : z.g[v].rrs) {
						if (record.get_type() == RRType::CNAME) {
							answers.clear();
							matchingRRs.clear();
							matchingRRs.push_back(record);
							//Return type is Ans if only CNAME is requested
							if (query.rrTypes[RRType::CNAME] && query.rrTypes.count() == 1) {
								answers.push_back(std::make_tuple(ReturnTag::ANS, nodeRRtypes, matchingRRs));
							}
							else {
								answers.push_back(std::make_tuple(ReturnTag::ANSQ, nodeRRtypes, matchingRRs));
							}
							return boost::make_optional(answers); //If CNAME then other records would be ignored.
						}
						if (query.rrTypes[record.get_type()] == 1) {
							matchingRRs.push_back(record);
							queryTypesFound.set(record.get_type());
						}
						//NS records at a wildcard node are forbidden.
					}
					if (queryTypesFound.count())answers.push_back(std::make_tuple(ReturnTag::ANS, queryTypesFound, matchingRRs));
					if ((queryTypesFound ^ query.rrTypes).count()) {
						answers.push_back(std::make_tuple(ReturnTag::ANS, queryTypesFound ^ query.rrTypes, vector<ResourceRecord> {}));
					}
					return boost::make_optional(answers);
				}
			}
			answers.push_back(std::make_tuple(ReturnTag::NX, query.rrTypes, vector<ResourceRecord> {}));
			return boost::make_optional(answers);
		}
		else {
			//Exact Query Match d_r = d_q

			std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes(z.g[closestEncloser].rrs);
			vector<ResourceRecord> matchingRRs; //All the RRs requested by query types except NS
			vector<ResourceRecord> NSRecords;
			std::bitset<RRType::N> queryTypesFound;
			for (auto& record : z.g[closestEncloser].rrs) {
				if (record.get_type() == RRType::NS) {
					NSRecords.push_back(record);
				}
				else if (query.rrTypes[record.get_type()] == 1) {
					queryTypesFound.set(record.get_type());
					matchingRRs.push_back(record);
				}
				if (record.get_type() == RRType::CNAME) {
					// CNAME Case
					answers.clear();
					matchingRRs.clear();
					matchingRRs.push_back(record);
					//Return type is Ans if only CNAME is requested
					if (query.rrTypes[RRType::CNAME] && query.rrTypes.count() == 1) {
						answers.push_back(std::make_tuple(ReturnTag::ANS, nodeRRtypes, matchingRRs));
					}
					else {
						answers.push_back(std::make_tuple(ReturnTag::ANSQ, nodeRRtypes, matchingRRs));
					}
					return boost::make_optional(answers); //If CNAME then other records would be ignored.
				}
			}
			// If there are NS records, then get their glue records too.
			if (nodeRRtypes[RRType::NS]) {
				vector<ResourceRecord> IPrecords = GlueRecordsLookUp(z.g, z.startVertex, NSRecords, z.domainChildLabelMap);
				NSRecords.insert(NSRecords.end(), IPrecords.begin(), IPrecords.end());
			}
			// Referral case
			if (nodeRRtypes[RRType::NS] && !nodeRRtypes[RRType::SOA]) {
				answers.push_back(std::make_tuple(ReturnTag::REF, nodeRRtypes, NSRecords));
				return boost::make_optional(answers);
			}
			//Add the NS and glue records if the user requested them.
			if (query.rrTypes[RRType::NS] && NSRecords.size()) {
				matchingRRs.insert(matchingRRs.end(), NSRecords.begin(), NSRecords.end());
				queryTypesFound.set(RRType::NS);
			}
			// Exact Type match 
			if (queryTypesFound.count()) answers.push_back(std::make_tuple(ReturnTag::ANS, queryTypesFound, matchingRRs));
			if ((queryTypesFound ^ query.rrTypes).count()) {
				answers.push_back(std::make_tuple(ReturnTag::ANS, queryTypesFound ^ query.rrTypes, vector<ResourceRecord> {}));
			}
			return boost::make_optional(answers);
		}
	}
}
