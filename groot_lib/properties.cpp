#include "properties.h"
#include <numeric>
#include <utility>

const int ECConsumerCount = 8;

std::atomic<long> gECcount(0);
vector<NodeFunction> gNodeFunctions;
vector<PathFunction> gPathFunctions;
moodycamel::ConcurrentQueue<EC> gECQueue;
std::atomic<bool> gDoneECgeneration(false);
moodycamel::ConcurrentQueue<json> gJsonQueue;

void CheckResponseReturned(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq) {
	/*
	  The set of end nodes is given and checks if some non-empty response is received from all the end nodes for all the requested types.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	*/
	bool incomplete = false;
	for (auto vd : endNodes) {
		if ((graph[vd].query.rrTypes & typesReq).count() > 0 && graph[vd].ns != "") {
			boost::optional<vector<ZoneLookUpAnswer>> answer = graph[vd].answer;
			if (answer) {
				if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED || std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {
					incomplete = true;
				}
				else {
					for (auto a : answer.get()) {
						if (!std::get<2>(a).size()) {
							json tmp;
							tmp["Property"] = "ResponseReturned";
							tmp["Equivalence Class"] = QueryFormat(graph[vd].query);
							tmp["Violation"] = "There was no response for T:" + RRTypesToString(std::get<1>(a) & typesReq) + " at name server:" + graph[vd].ns;
							gJsonQueue.enqueue(tmp);
						}
					}
				}
			}
			else {
				cout << "Implementation Error: CheckResponseReturned - A node in the interpretation graph with empty answer" << endl;
			}
		}
	}
}

std::bitset<RRType::N> CompareResponse(const vector<ResourceRecord>& resA, const vector<ResourceRecord>& resB, std::bitset<RRType::N> typesReq) {
	//Can be optimized
	std::bitset<RRType::N> typesDiff;
	for (auto record : resA) {
		if (typesReq[record.get_type()]) {
			bool found = false;
			for (auto r : resB) {
				if (record.get_name() == r.get_name() && record.get_rdata() == r.get_rdata() && record.get_ttl() == r.get_ttl()) {
					found = true;
					break;
				}
			}
			if (!found) {
				typesDiff.set(record.get_type());
			}
		}

	}
	for (auto record : resB) {
		if (typesReq[record.get_type()]) {
			bool found = false;
			for (auto r : resA) {
				if (record.get_name() == r.get_name() && record.get_rdata() == r.get_rdata() && record.get_ttl() == r.get_ttl()) {
					found = true;
					break;
				}
			}
			if (!found) {
				typesDiff.set(record.get_type());
			}
		}
	}
	return typesDiff;
}

void CheckSameResponseReturned(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq) {
	/*
	  The set of end nodes is given and checks if same response is received from all the end nodes.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	*/
	boost::optional<vector<ZoneLookUpAnswer>> response;
	bool incomplete = false;
	bool foundDiff = false;
	for (auto vd : endNodes) {
		if ((graph[vd].query.rrTypes & typesReq).count() > 0 && graph[vd].ns != "") {
			boost::optional<vector<ZoneLookUpAnswer>> answer = graph[vd].answer;
			if (answer) {
				if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED || std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {
					incomplete = true;
				}
				else if (!response) {
					response = answer;
				}
				else {
					CommonSymDiff diff = CompareRRs(std::get<2>(answer.get()[0]), std::get<2>(response.get()[0]));
					if (std::get<1>(diff).size() || std::get<2>(diff).size()) {
						foundDiff = true;
					}
				}
			}
			else {
				cout << "Implementation Error: CheckSameResponseReturned - A node in the interpretation graph with empty answer" << endl;
			}
		}
	}
	if (foundDiff) {
		json tmp;
		tmp["Property"] = "ResponseConsistency";
		tmp["Equivalence Class"] = QueryFormat(graph[endNodes[0]].query);
		tmp["Violation"] = "Difference in responses found";
		gJsonQueue.enqueue(tmp);
	}
}

void PrettyPrintResponseValue(set<string> response, set<string>& value, const InterpreterGraph& graph, const IntpVD& node, json& tmp) {

	if (tmp.find("Mismatches") == tmp.end()) {
		tmp["Mismatches"] = {};
	}
	//printf(ANSI_COLOR_RED     "Response Mismatch:"     ANSI_COLOR_RESET "\n");
	json j;
	j["Found"] = std::accumulate(response.begin(), response.end(), std::string{});
	j["Equivalence class"] = QueryFormat(graph[node].query);
	j["At NS"] = graph[node].ns;
	tmp["Mismatches"].push_back(j);
}

void CheckResponseValue(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq, set<string> values) {
	/*
	  The set of end nodes is given and if the return tag is Ans then compare with the user input value.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	*/
	bool foundDiff = false;
	bool incomplete = false;
	json tmp;
	for (auto vd : endNodes) {
		if ((graph[vd].query.rrTypes & typesReq).count() > 0) {
			if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::ANS) {
				set<string> rdatas;
				for (auto rr : std::get<2>(graph[vd].answer.get()[0])) {
					if (typesReq[rr.get_type()] == 1) {
						rdatas.insert(rr.get_rdata());
					}
				}
				if (rdatas != values) {
					PrettyPrintResponseValue(rdatas, values, graph, vd, tmp);
				}
			}
			else if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NX) {
				PrettyPrintResponseValue(set<string> {}, values, graph, vd, tmp);
			}
			else if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
				std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REFUSED) {
				incomplete = true;
			}
		}
	}
	if (!tmp.empty()) {
		std::string s = std::accumulate(values.begin(), values.end(), std::string{});
		tmp["Expected"] = s;
		tmp["Property"] = "Response Value";
		gJsonQueue.enqueue(tmp);
	}
}

void NumberOfRewrites(const InterpreterGraph& graph, const Path& p, int num_rewrites) {
	/*
		Number of Rewrites = Number of nodes on the path with return tag AnsQ
	*/
	bool incomplete = false;
	int rewrites = 0;
	for (auto vd : p) {
		if (graph[vd].ns != "") {
			if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REWRITE) {
				rewrites++;
			}
			else if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
				std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REFUSED) {
				incomplete = true;
			}
		}
	}
	if (rewrites > num_rewrites) {
		json tmp;
		tmp["Property"] = "Rewrites";
		tmp["Equivalence Class"] = QueryFormat(graph[p[0]].query);
		std::ostringstream stringStream;
		stringStream << "Number of rewrites (" << rewrites << ") exceeded " << num_rewrites;
		tmp["Violation"] = stringStream.str();
		gJsonQueue.enqueue(tmp);
	}
}

void NumberOfHops(const InterpreterGraph& graph, const Path& p, int num_hops) {
	/*
		Number of Hops = Number of name servers in the path.
		We may encounter a Node with Refused/NSnotfound in which case we have incomplete information.
	*/
	vector<string> nameServers;
	bool incomplete = false;
	for (auto vd : p) {
		if (graph[vd].ns != "" && (!nameServers.size() || graph[vd].ns != nameServers.back())) {
			nameServers.push_back(graph[vd].ns);
			if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
				std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REFUSED) {
				incomplete = true;
			}
		}
	}
	if (nameServers.size() > num_hops) {
		json tmp;
		tmp["Property"] = "Hops";
		tmp["Equivalence Class"] = QueryFormat(graph[p[0]].query);
		std::ostringstream stringStream;
		stringStream << "Number of hops (" << nameServers.size() << ") exceeded " << num_hops;
		tmp["Violation"] = stringStream.str();
		gJsonQueue.enqueue(tmp);
	}
}

string QueryFormat(const EC& query) {
	string q = "";
	if (query.excluded) {
		q += "~{ }.";
	}
	else {
		q += "";
	}
	q += LabelsToString(query.name);
	return q;
}

tuple<vector<ResourceRecord>, vector<ResourceRecord>> GetNSGlueRecords(const vector<ResourceRecord>& records) {
	// From the input set of records, return the NS records and IP records
	vector<ResourceRecord> nsRecords;
	vector<ResourceRecord> glueRecords;
	for (auto r : records) {
		if (r.get_type() == RRType::NS)nsRecords.push_back(r);
		if (r.get_type() == RRType::A || r.get_type() == RRType::AAAA)glueRecords.push_back(r);
	}
	return std::make_tuple(nsRecords, glueRecords);
}

void CheckDelegationConsistency(const InterpreterGraph& graph, const Path& p)
{
	/*
	  The path should have at least three nodes including the dummy node.
	  There should be consective nodes answering for the given query, more specifically, the parent should have Ref type and the child with Ans type.
	  If there is no such pair then the given query name is not a zone cut and delegation consistency is not a valid property to check.
	*/
	if (p.size() > 2) {
		if (graph[p[0]].ns == "") {
			const EC& query = graph[p[0]].query;
			int parentIndex = -1;
			for (int i = 0; i < p.size(); i++) {
				if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REF) {
					// Parent found
					if (i < p.size() - 1 && graph[p[static_cast<long long>(i) + 1]].answer) {
						auto& ans = graph[p[static_cast<long long>(i) + 1]].answer.get()[0];
						if (std::get<0>(ans) == ReturnTag::ANS &&
							std::get<1>(ans)[RRType::NS] &&
							std::get<2>(ans).size() > 0 &&
							query.name == graph[p[static_cast<long long>(i) + 1]].query.name) {
							// Child found (should have at least one NS record) and also the query matches
							parentIndex = i;
						}
					}
				}
			}
			if (parentIndex != -1) {
				tuple<vector<ResourceRecord>, vector<ResourceRecord>> parentRecords = GetNSGlueRecords(std::get<2>(graph[p[parentIndex]].answer.get()[0]));
				tuple<vector<ResourceRecord>, vector<ResourceRecord>> childRecords = GetNSGlueRecords(std::get<2>(graph[p[static_cast<long long>(parentIndex) + 1]].answer.get()[0]));
				CommonSymDiff nsDiff = CompareRRs(std::get<0>(parentRecords), std::get<0>(childRecords));
				CommonSymDiff glueDiff = CompareRRs(std::get<1>(parentRecords), std::get<1>(childRecords));
				if (std::get<1>(nsDiff).size() || std::get<2>(nsDiff).size()) {
					json tmp;
					tmp["Property"] = "Delegation Consistency";
					tmp["Equivalence Class"] = QueryFormat(query);
					tmp["Violation"] = "Inconsistency in NS records  at " + graph[p[parentIndex]].ns + " and " + graph[p[static_cast<long long>(parentIndex) + 1]].ns;
					gJsonQueue.enqueue(tmp);
				}
				if (std::get<1>(glueDiff).size() || std::get<2>(glueDiff).size()) {
					json tmp;
					tmp["Property"] = "Delegation Consistency";
					tmp["Equivalence Class"] = QueryFormat(query);
					tmp["Violation"] = "Inconsistency in Glue records  at " + graph[p[parentIndex]].ns + " and " + graph[p[static_cast<long long>(parentIndex) + 1]].ns;
					gJsonQueue.enqueue(tmp);
				}
			}
		}
		else {
			cout << "Implementation Error: CheckDelegationConsistency - The path does not start at the dummy node" << endl;
		}
	}
}

void CheckLameDelegation(const InterpreterGraph& graph, const Path& p)
{
	/*
	  The path should have at least three nodes including the dummy node.
	  There should be consective nodes answering for the given query, more specifically, the parent should have Ref type and the child with Refused/NSnotfound type.
	  If there is such a pair then from the given zone fileswe flag it as lame delegation which may be a false positive if we didn't have access to all zone files at the child NS.
	*/
	if (p.size() > 2) {
		if (graph[p[0]].ns == "") {
			const EC& query = graph[p[0]].query;
			int parentIndex = -1;
			for (int i = 0; i < p.size(); i++) {
				if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REF) {
					// Parent found
					if (i < p.size() - 1 && graph[p[static_cast<long long>(i) + 1]].answer) {
						auto& ans = graph[p[static_cast<long long>(i) + 1]].answer.get()[0];
						if ((std::get<0>(ans) == ReturnTag::REFUSED) && query.name == graph[p[static_cast<long long>(i) + 1]].query.name) {
							//if ((std::get<0>(ans) == ReturnTag::REFUSED || std::get<0>(ans) == ReturnTag::NSNOTFOUND) && query.name == graph[p[static_cast<long long>(i) + 1]].query.name) {
								// Child found and also the query matches
							json tmp;
							tmp["Property"] = "Lame Delegation";
							tmp["Equivalence Class"] = QueryFormat(query);
							tmp["Violation"] = "Inconsistency at " + graph[p[i]].ns + " and " + graph[p[static_cast<long long>(i) + 1]].ns;
							gJsonQueue.enqueue(tmp);
						}
					}
				}
			}
		}
		else {
			cout << "Implementation Error: CheckDelegationConsistency - The path does not start at the dummy node" << endl;
		}
	}
}

bool CheckSubDomain(vector<Label>& domain, vector<Label>  queryLabels) {

	if (domain.size() > queryLabels.size()) {
		return false;
	}
	for (int i = 0; i < domain.size(); i++) {
		if (!(domain[i] == queryLabels[i])) {
			return false;
		}
	}
	return true;
}

void QueryRewrite(const InterpreterGraph& graph, const Path& p, vector<Label> domain)
{
	/*
	  If there is a node with answer tag as AnsQ then the new query should be under the subdomain of domain.
	*/
	for (int i = 0; i < p.size(); i++) {
		if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
			// Rewrite happened at this node
			//The answer would have a DNAME/CNAME Resource record and the new query would be availble in the next node in the path.
			if (i < p.size() - 1) {
				if (!CheckSubDomain(domain, graph[p[static_cast<long long>(i) + 1]].query.name)) {
					json tmp;
					tmp["Property"] = "Query Rewrite";
					tmp["Equivalence Class"] = QueryFormat(graph[p[i]].query);
					tmp["Violation"] = "Query is rewritten to " + QueryFormat(graph[p[static_cast<long long>(i) + 1]].query) + " at NS:" + graph[p[i]].ns + " which is not under " + LabelsToString(domain);
					gJsonQueue.enqueue(tmp);
				}
			}
			else {
				cout << "Implementation Error: QueryRewrite - REWRITE is the last node in the path:" << QueryFormat(graph[p[0]].query) << endl;
			}
		}
	}
}

void NameServerContact(const InterpreterGraph& graph, const Path& p, vector<Label> domain)
{
	/*
	 At any point in the resolution process, the query should not be sent to a name server outside the domain.
	*/
	for (int i = 0; i < p.size(); i++) {
		if (graph[p[i]].ns != "") {
			if (!CheckSubDomain(domain, GetLabels(graph[p[i]].ns))) {
				json tmp;
				tmp["Property"] = "Name Server Contact";
				tmp["Equivalence Class"] = QueryFormat(graph[p[i]].query);
				tmp["Violation"] = "Query is sent to NS:" + graph[p[i]].ns + " which is not under " + LabelsToString(domain);
				gJsonQueue.enqueue(tmp);
			}
		}
	}
}

CommonSymDiff CompareRRs(vector<ResourceRecord> resA, vector<ResourceRecord> resB) {
	//For the given pair of collection of resource records, return the common RR's, RR's present only in A and RR's present only in B.
	// Assumption: resA and resB has unique records (no two records in either vector are exactly the same)
	vector<ResourceRecord> common;
	auto it = resA.begin();
	while (it != resA.end()) {
		auto itb = resB.begin();
		bool erased = false;
		while (itb != resB.end()) {
			if (*it == *itb) {
				common.push_back(*it);
				it = resA.erase(it);
				resB.erase(itb);
				erased = true;
				break;
			}
			else {
				itb++;
			}
		}
		if (!erased)it++;
	}
	return std::make_tuple(common, resA, resB);
}

void ConstructOutputNS(json& j, CommonSymDiff& nsDiff, boost::optional<CommonSymDiff> glueDiff, string serverA, string serverB, string a, string b) {
	if (std::get<1>(nsDiff).empty() && std::get<2>(nsDiff).empty() && ((glueDiff && std::get<1>(glueDiff.get()).empty() && std::get<2>(glueDiff.get()).empty()) || !glueDiff)) {
		return;
	}
	if (j.find("Inconsistent Pairs") == j.end()) {
		j["Inconsistent Pairs"] = {};
	}
	json diffAB;
	diffAB[a + " NS"] = serverA;
	diffAB[b + " NS"] = serverB;
	if (!std::get<0>(nsDiff).empty()) {
		diffAB["Common NS Records"] = {};
		for (auto r : std::get<0>(nsDiff)) {
			diffAB["Common NS Records"].push_back(r.toString());
		}
	}
	if (!std::get<1>(nsDiff).empty()) {
		diffAB["Exclusive " + a + " NS Records"] = {};
		for (auto r : std::get<1>(nsDiff)) {
			diffAB["Exclusive " + a + " NS Records"].push_back(r.toString());
		}
	}
	if (!std::get<2>(nsDiff).empty()) {
		diffAB["Exclusive " + b + " NS Records"] = {};
		for (auto r : std::get<2>(nsDiff)) {
			diffAB["Exclusive " + b + " NS Records"].push_back(r.toString());
		}
	}
	if (glueDiff) {
		if (!std::get<0>(glueDiff.get()).empty()) {
			diffAB["Common Glue Records"] = {};
			for (auto r : std::get<0>(glueDiff.get())) {
				diffAB["Common Glue Records"].push_back(r.toString());
			}
		}
		if (!std::get<1>(glueDiff.get()).empty()) {
			diffAB["Exclusive " + a + " Glue Records"] = {};
			for (auto r : std::get<1>(glueDiff.get())) {
				diffAB["Exclusive " + a + " Glue Records"].push_back(r.toString());
			}
		}
		if (!std::get<2>(glueDiff.get()).empty()) {
			diffAB["Exclusive " + b + " Glue Records"] = {};
			for (auto r : std::get<2>(glueDiff.get())) {
				diffAB["Exclusive " + b + " Glue Records"].push_back(r.toString());
			}
		}
	}
	j["Inconsistent Pairs"].push_back(diffAB);
}

void ParentChildNSRecordsComparison(std::vector<ZoneIdGlueNSRecords>& parent, std::vector<ZoneIdGlueNSRecords>& child, string userInput) {
	// ZoneId, GlueRecords, NSRecords
	json j;
	std::map<int, string> zoneIdToNS;
	for (auto p : parent) {
		zoneIdToNS.try_emplace(std::get<0>(p), SearchForNameServer(std::get<0>(p)));
		for (auto c : child) {
			zoneIdToNS.try_emplace(std::get<0>(c), SearchForNameServer(std::get<0>(c)));
			auto nsDiff = CompareRRs(std::get<2>(p), std::get<2>(c));
			if (std::get<1>(c)) {
				auto glueDiff = CompareRRs(std::get<1>(p).get(), std::get<1>(c).get());
				ConstructOutputNS(j, nsDiff, glueDiff, zoneIdToNS.at(std::get<0>(p)), zoneIdToNS.at(std::get<0>(c)), "parent", "child");
			}
			else {
				ConstructOutputNS(j, nsDiff, {}, zoneIdToNS.at(std::get<0>(p)), zoneIdToNS.at(std::get<0>(c)), "parent", "child");
			}
		}
	}
	for (auto it = parent.begin(); it != parent.end(); ++it) {
		for (auto itp = it + 1; itp != parent.end(); ++itp) {
			auto nsDiff = CompareRRs(std::get<2>(*it), std::get<2>(*itp));
			auto glueDiff = CompareRRs(std::get<1>(*it).get(), std::get<1>(*itp).get());
			ConstructOutputNS(j, nsDiff, glueDiff, zoneIdToNS.at(std::get<0>(*it)), zoneIdToNS.at(std::get<0>(*itp)), "parent-a", "parent-b");
		}
	}
	for (auto it = child.begin(); it != child.end(); ++it) {
		for (auto itp = it + 1; itp != child.end(); ++itp) {
			auto nsDiff = CompareRRs(std::get<2>(*it), std::get<2>(*itp));
			auto glueDiff = CompareRRs(std::get<1>(*it).get_value_or({}), std::get<1>(*itp).get_value_or({}));
			ConstructOutputNS(j, nsDiff, glueDiff, zoneIdToNS.at(std::get<0>(*it)), zoneIdToNS.at(std::get<0>(*itp)), "child-a", "child-b");
		}
	}
	if (j.size() || (parent.empty() && !child.empty())) {
		j["Property"] = "Structural Delegation Consistency";
		j["Domain Name"] = userInput;
		if (parent.empty() && !child.empty()) {
			j["Warning"] = "There are no NS records at the parent or parent zone file is missing";
			j["Child NS"] = {};
			for (auto c : child) j["Child NS"].push_back(SearchForNameServer(std::get<0>(c)));
		}
		gJsonQueue.enqueue(j);
	}
}

void CheckStructuralDelegationConsistency(LabelGraph& graph, VertexDescriptor root, string userInput, boost::optional<VertexDescriptor> labelNode)
{
	VertexDescriptor node;
	if (!labelNode) {
		vector<Label> labels = GetLabels(userInput);
		vector<closestNode> closestEnclosers = SearchNode(graph, root, labels, 0);
		if (closestEnclosers.size() && closestEnclosers[0].second == labels.size()) {
			node = closestEnclosers[0].first;
		}
		else {
			cout << "User Input not found in the label graph" << endl;
			return;
		}
	}
	else {
		node = labelNode.get();
	}
	auto tups = graph[node].zoneIdVertexId;
	auto types = graph[node].rrTypesAvailable;
	if (types[RRType::NS] == 1) {
		std::vector<ZoneIdGlueNSRecords> parents;
		std::vector<ZoneIdGlueNSRecords> children;
		for (auto p : tups) {
			if (gZoneIdToZoneMap.find(std::get<0>(p)) != gZoneIdToZoneMap.end()) {
				Zone& z = gZoneIdToZoneMap.find(std::get<0>(p))->second;
				vector<ResourceRecord> nsRecords;
				bool soa = false;
				for (auto record : z.g[std::get<1>(p)].rrs) {
					if (record.get_type() == RRType::SOA) soa = true;
					if (record.get_type() == RRType::NS) nsRecords.push_back(record);
				}
				if (soa) {
					/*
						The RequireGlueRecords check is necessary as the child may not need glue records but the parent might in which case we
						don't need to compare the glue record consistency. This case might arise for example in ucla.edu the NS for zone cs.ucla.edu
						may be listed as ns1.ee.ucla.edu, then ucla.edu requires a glue record but not cs.ucla.edu zone.
					*/
					if (RequireGlueRecords(z, nsRecords)) {
						children.push_back(std::make_tuple(std::get<0>(p), GlueRecordsLookUp(z.g, z.startVertex, nsRecords, z.domainChildLabelMap), std::move(nsRecords)));
					}
					else {
						children.push_back(std::make_tuple(std::get<0>(p), boost::optional<vector<ResourceRecord>> {}, std::move(nsRecords)));
					}
				}
				else {
					parents.push_back(std::make_tuple(std::get<0>(p), GlueRecordsLookUp(z.g, z.startVertex, nsRecords, z.domainChildLabelMap), std::move(nsRecords)));
				}
			}
			else {
				cout << "In Function CheckStructuralDelegationConsistency: ZoneId not found ";
				exit(0);
			}
		}
		//Compare the children and parents records
		ParentChildNSRecordsComparison(parents, children, userInput);
	}
	else {

	}
}

void CheckAllStructuralDelegations(LabelGraph& graph, VertexDescriptor root, string userInput, VertexDescriptor currentNode) {

	string n = "";
	if (graph[currentNode].name.get() != "") {
		n = graph[currentNode].name.get() + "." + userInput;
	}
	CheckStructuralDelegationConsistency(graph, root, n, currentNode);
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(currentNode, graph))) {
		if (graph[edge].type == normal) {
			CheckAllStructuralDelegations(graph, root, n, edge.m_target);
		}
	}
}

void DFS(InterpreterGraph& graph, IntpVD start, Path p, vector<IntpVD>& endNodes, vector<std::function<void(const InterpreterGraph&, const Path&)>>& pathFunctions) {

	EC& query = graph[start].query;
	if (graph[start].ns != "") {
		// If the returnTag is a AnsQ (along with record being CNAME) and request type contains CNAME then the path ends here for CNAME and this node is a leaf node with respect to t = CNAME.
		if (graph[start].answer && graph[start].answer.get().size() > 0) {
			if (std::get<0>(graph[start].answer.get()[0]) == ReturnTag::REWRITE && std::get<2>(graph[start].answer.get()[0])[0].get_type() == RRType::CNAME && query.rrTypes[RRType::CNAME]) {
				if (endNodes.end() == std::find(endNodes.begin(), endNodes.end(), start)) {
					endNodes.push_back(start);
				}
				Path cnamePath = p;
				cnamePath.push_back(start);
				for (auto& f : pathFunctions) {
					f(graph, cnamePath);
				}
			}
		}
		else {
			cout << "Empty Answer found in the interpretation graph during DFS traversal";
		}
	}
	for (auto v : p) {
		if (v == start) {
			//cout << "loop detected for: " << LabelsToString(query.name) << endl;
			return;
		}
	}
	p.push_back(start);
	for (IntpED edge : boost::make_iterator_range(out_edges(start, graph))) {
		//TODO : Edges that have side query
		DFS(graph, edge.m_target, p, endNodes, pathFunctions);
	}
	if (out_degree(start, graph) == 0) {
		// Last node in the graph
		if (endNodes.end() == std::find(endNodes.begin(), endNodes.end(), start)) {
			endNodes.push_back(start);
		}
		//Path detected
		for (auto& f : pathFunctions) {
			f(graph, p);
		}
	}
}

void PrettyPrintLoop(InterpreterGraph& graph, IntpVD start, Path p) {
	Path loop;
	bool found = false;
	for (auto v : p) {
		if (v == start) {
			found = true;
		}
		if (found) loop.push_back(v);
	}
	json tmp;
	tmp["Property"] = "Cyclic Zone Dependency";
	tmp["Loop"] = {};
	for (auto v : loop) {
		json node;
		node["NS"] = graph[v].ns;
		node["Query"] = QueryFormat(graph[v].query);
		if (graph[v].answer) {
			node["AnswerTag"] = std::get<0>(graph[v].answer.get()[0]);
		}
		tmp["Loop"].push_back(node);
	}
	gJsonQueue.enqueue(tmp);
}

void LoopChecker(InterpreterGraph& graph, IntpVD start, Path p) {

	EC& query = graph[start].query;
	for (auto v : p) {
		if (v == start) {
			PrettyPrintLoop(graph, start, p);
			return;
		}
	}
	p.push_back(start);
	for (IntpED edge : boost::make_iterator_range(out_edges(start, graph))) {
		//TODO : Edges that have side query
		if (graph[edge].intermediateQuery) {
			LoopChecker(graph, graph[edge].intermediateQuery.get(), p);
		}
		LoopChecker(graph, edge.m_target, p);
	}
}

void CheckPropertiesOnEC(EC& query)
{
	gECcount++;
	//cout<<QueryFormat(query)<<endl;
	InterpreterGraphWrapper intGraphWrapper;
	BuildInterpretationGraph(query, intGraphWrapper);
	if (num_vertices(intGraphWrapper.intG) == 1) {
		cout << "No NameServer exists to resolve EC- " << QueryFormat(query) << endl;
		return;
	}
	vector<IntpVD> endNodes;
	//LoopChecker(intGraphWrapper.intG, intGraphWrapper.startVertex, Path{}, output);
	DFS(intGraphWrapper.intG, intGraphWrapper.startVertex, Path{}, endNodes, gPathFunctions);
	/*if (intGraphWrapper.intG.m_vertices.size() > 5) {
		GenerateDotFileInterpreter("Int.dot", intGraphWrapper.intG);
	}
	*/
	for (auto f : gNodeFunctions) {
		f(intGraphWrapper.intG, endNodes);
	}
}

vector<closestNode> SearchNode(LabelGraph& g, VertexDescriptor closestEncloser, vector<Label>& labels, int index) {
	/*
		Given a user input the functions returns all the closest enclosers along with the number of labels matched.
		The function returns only the longest matching enclosers as the smaller ones would be automatically dealt by the ECs generated from longest ones.
	*/
	vector<closestNode> enclosers;
	if (labels.size() == index) {
		enclosers.push_back(closestNode{ closestEncloser, index });
		return enclosers;
	}
	if (index == g[closestEncloser].len) {
		//Loop detected
		//TODO: Trace the loop to get the usr input.
		enclosers.push_back(closestNode{ closestEncloser, -1 });
		return enclosers;
	}

	int16_t before_len = g[closestEncloser].len;
	g[closestEncloser].len = index;
	// The current node could also be the closest encloser if no child matches.
	enclosers.push_back(closestNode{ closestEncloser, index });
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closestEncloser, g))) {
		if (g[edge].type == dname) {
			auto r = SearchNode(g, edge.m_target, labels, index);
			enclosers.insert(enclosers.end(), r.begin(), r.end());
		}
	}
	if (gDomainChildLabelMap.find(closestEncloser) != gDomainChildLabelMap.end()) {
		//Check if a hash-map exists for child labels.
		LabelMap& m = gDomainChildLabelMap.find(closestEncloser)->second;
		auto it = m.find(labels[index]);
		if (it != m.end()) {
			index++;
			auto r = SearchNode(g, it->second, labels, index);
			enclosers.insert(enclosers.end(), r.begin(), r.end());
		}
		else {
			//label[index] does not exist
		}
	}
	else {
		for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closestEncloser, g))) {
			if (g[edge].type == normal) {
				if (g[edge.m_target].name == labels[index]) {
					index++;
					auto r = SearchNode(g, edge.m_target, labels, index);
					enclosers.insert(enclosers.end(), r.begin(), r.end());
					break;
				}
			}
		}
	}
	g[closestEncloser].len = before_len;
	int max = -1;
	for (auto& r : enclosers) {
		if (r.second > max) {
			max = r.second;
		}
	}
	vector<closestNode> actualEnclosers;
	if (max != -1) {
		for (auto& r : enclosers) {
			if (r.second == max) {
				actualEnclosers.push_back(r);
			}
		}
	}
	return actualEnclosers;
}

void WildCardChildEC(std::vector<Label>& childrenLabels, vector<Label>& labels, std::bitset<RRType::N>& typesReq, int index) {

	EC wildCardMatch;
	wildCardMatch.name.clear();
	for (int i = 0; i < index; i++) {
		wildCardMatch.name.push_back(labels[i]);
	}
	wildCardMatch.rrTypes = typesReq;
	wildCardMatch.excluded = boost::make_optional(std::move(childrenLabels));
	//EC generated - Push it to the global queue
	gECQueue.enqueue(wildCardMatch);
}

void NodeEC(LabelGraph& g, vector<Label>& name, std::bitset<RRType::N>& typesReq, bool checkDirectly) {

	EC present;
	present.name = name;
	present.rrTypes = typesReq;
	//EC generated
	if (checkDirectly)CheckPropertiesOnEC(present);
	else
		//Push it to the global queue
		gECQueue.enqueue(present);
}

void SubDomainECGeneration(LabelGraph& g, VertexDescriptor start, vector<Label> parentDomainName, std::bitset<RRType::N> typesReq, bool skipLabel) {

	int len = 0;
	for (Label l : parentDomainName) {
		len += l.get().length() + 1;
	}
	if (len == kMaxDomainLength) {
		return;
	}
	Label nodeLabel = g[start].name;
	vector<Label> name;
	name = parentDomainName;
	if (nodeLabel.get() == ".") {
		//Root has empty parent vector
	}
	else if (parentDomainName.size() == 0) {
		//Empty name vector implies root
		name.push_back(nodeLabel);
	}
	else if (!skipLabel) {
		//DNAME can not occur at the root.
		name.push_back(nodeLabel);
	}
	int16_t beforeLen = g[start].len;
	int nodeLen = 0;
	for (Label l : name) {
		nodeLen += l.get().length() + 1;
	}
	g[start].len = nodeLen;
	std::vector<Label> childrenLabels;
	std::optional<VertexDescriptor>  wildcardNode;

	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(start, g))) {
		if (g[edge].type == normal) {
			if (g[edge.m_target].name.get() == "*") {
				wildcardNode = edge.m_target;
			}
			SubDomainECGeneration(g, edge.m_target, name, typesReq, false);
		}
		if (g[edge].type == dname) {
			SubDomainECGeneration(g, edge.m_target, name, typesReq, true);
		}
	}
	g[start].len = beforeLen;
	if (!skipLabel) {
		// EC for the node if the node is not skipped.
		NodeEC(g, name, typesReq, false);
	}

	// wildcardNode is useful when we want to generate only positive queries and avoid the negations.
	if (wildcardNode) {
		WildCardChildEC(childrenLabels, name, typesReq, name.size());
	}
	else {
		//Non-existent child category
		EC nonExistent;
		nonExistent.name = name;
		nonExistent.rrTypes.flip();
		nonExistent.excluded = boost::make_optional(std::move(childrenLabels));
		//EC generated
		//CheckPropertiesOnEC(nonExistent, output);
		//Push it to the global queue
		gECQueue.enqueue(nonExistent);
	}
}

void GenerateECAndCheckProperties(LabelGraph& g, VertexDescriptor root, string userInput, std::bitset<RRType::N> typesReq, bool subdomain, json& output) {
	//Given an user input for domain and query types, the function searches for relevant node
	// The search is relevant even for subdomain = False as we want to know the exact EC
	if (userInput.length() > kMaxDomainLength) {
		cout << userInput << " is an invalid domain name (length exceedes)" << endl;
	}
	vector<Label> labels = GetLabels(userInput);
	for (Label& l : labels) {
		if (l.get().length() > kMaxLabelLength) {
			cout << userInput << " is an invalid domain name as " << l.get() << " exceedes length" << endl;
		}
	}
	vector<closestNode> closestEnclosers = SearchNode(g, root, labels, 0);
	if (closestEnclosers.size()) {
		//cross-check
		int matchedIndex = closestEnclosers[0].second;
		for (auto& r : closestEnclosers) {
			if (r.second != matchedIndex) {
				cout << "Error: Multiple ClosestEnclosers with different lengths(Function: GenerateECAndCheckProperties)" << endl;
				exit(0);
			}
		}
		if (labels.size() == matchedIndex) {
			if (subdomain == true) {
				vector<Label> parentDomainName = labels;
				parentDomainName.pop_back();
				vector<std::thread> ECproducers;
				std::thread ECconsumers[ECConsumerCount];
				std::atomic<int> doneConsumers(0);
				if (gECQueue.size_approx() != 0) {
					cout << " The global EC queue is non-empty in GenerateECAndCheckProperties";
				}
				//EC producer threads
				for (auto& encloser : closestEnclosers) {
					ECproducers.push_back(thread([&]() {
						SubDomainECGeneration(g, encloser.first, parentDomainName, typesReq, false);
						}
					));
				}
				//EC consumer threads which are also JSON producer threads
				for (int i = 0; i != ECConsumerCount; ++i) {
					ECconsumers[i] = thread([i, &output, &doneConsumers]() {
						EC item;
						bool itemsLeft;
						int id = i;
						do {
							itemsLeft = !gDoneECgeneration;
							while (gECQueue.try_dequeue(item)) {
								itemsLeft = true;
								//cout << "Id:" << id << " EC:"<< QueryFormat(item) << endl;
								CheckPropertiesOnEC(item);
							}
						} while (itemsLeft || doneConsumers.fetch_add(1, std::memory_order_acq_rel) + 1 == ECConsumerCount);
						});
				}
				//JSON consumer thread
				std::thread jsonConsumer = thread([&]() {
					json item;
					bool itemsLeft;
					do {
						itemsLeft = doneConsumers.load(std::memory_order_acquire) != ECConsumerCount;
						while (gJsonQueue.try_dequeue(item)) {
							itemsLeft = true;
							output.push_back(item);
						}
					} while (itemsLeft);
					});
				for (auto& t : ECproducers) {
					t.join();
				}
				gDoneECgeneration = true;
				for (int i = 0; i != ECConsumerCount; ++i) {
					ECconsumers[i].join();
				}
				jsonConsumer.join();
			}
			else {
				NodeEC(g, labels, typesReq, true);
			}
		}
		else {
			//Sub-domain queries can not be peformed.
			if (subdomain) {
				cout << "The complete domain: " << userInput << " doesn't exist so sub-domain is not valid";
			}
			// The query might match a "wildcard" or its part of non-existent child nodes. We just set excluded to know there is some negation set there.
			EC nonExistent;
			nonExistent.name.clear();
			for (int i = 0; i < matchedIndex; i++) {
				nonExistent.name.push_back(labels[i]);
			}
			nonExistent.rrTypes = typesReq;
			nonExistent.excluded = boost::make_optional(std::vector<Label>());
			//EC generated
			CheckPropertiesOnEC(nonExistent);
		}
		// Pop of from the JSON queue when multi-threading is not used.
		json item;
		while (gJsonQueue.try_dequeue(item)) {
			output.push_back(item);
		}
	}
	else {
		cout << "Error: No ClosestEnclosers found(Function: GenerateECAndCheckProperties)" << endl;
		exit(0);
	}
}