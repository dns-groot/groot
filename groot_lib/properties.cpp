#include "properties.h"
#include <numeric>
#include <utility>

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
							cout << "There was no response " << QueryFormat(graph[vd].query) << " for T:" << RRTypesToString(std::get<1>(a) & typesReq);
							cout << " at name server: " << graph[vd].ns << endl;
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
						cout << "Difference in responses found " << QueryFormat(graph[vd].query) << endl;
					}
				}
			}
			else {
				cout << "Implementation Error: CheckSameResponseReturned - A node in the interpretation graph with empty answer" << endl;
			}
		}
	}
}

void PrettyPrintResponseValue(set<string> response, set<string>& value, const InterpreterGraph& graph, const IntpVD& node) {

	std::string s = std::accumulate(value.begin(), value.end(), std::string{});
	//printf(ANSI_COLOR_RED     "Response Mismatch:"     ANSI_COLOR_RESET "\n");
	cout << "Response Mismatch:" << endl;
	cout << "\t Expected: " << s << endl;
	cout << "\t Found: " << std::accumulate(response.begin(), response.end(), std::string{}) << endl;;
	cout << "\t At NS: " << graph[node].ns << endl;
	cout << "\t" << QueryFormat(graph[node].query) << endl;
}

void CheckResponseValue(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq, set<string> values) {
	/*
	  The set of end nodes is given and if the return tag is Ans then compare with the user input value.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	*/
	bool foundDiff = false;
	bool incomplete = false;
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
					PrettyPrintResponseValue(rdatas, values, graph, vd);
				}
			}
			else if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NX) {
				PrettyPrintResponseValue(set<string> {}, values, graph, vd);
			}
			else if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
				std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REFUSED) {
				incomplete = true;
			}
		}
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
			if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::ANSQ) {
				rewrites++;
			}
			else if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
				std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REFUSED) {
				incomplete = true;
			}
		}
	}
	if (rewrites > num_rewrites) {
		cout << " Number of rewrites exceeded " << num_rewrites << QueryFormat(graph[p[0]].query) << endl;
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
		cout << " Number of hops exceeded " << num_hops << QueryFormat(graph[p[0]].query) << endl;
	}
}

string QueryFormat(const EC& query) {
	string q = "";
	if (query.excluded) {
		q += " for Q: ~{ }.";
	}
	else {
		q += " for Q: ";
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
					if (i < p.size() - 1 && graph[p[i + 1]].answer) {
						auto& ans = graph[p[i + 1]].answer.get()[0];
						if (std::get<0>(ans) == ReturnTag::ANS &&
							std::get<1>(ans)[RRType::NS] &&
							std::get<2>(ans).size() > 0 &&
							query.name == graph[p[i + 1]].query.name) {
							// Child found (should have at least one NS record) and also the query matches
							parentIndex = i;
						}
					}
				}
			}
			if (parentIndex != -1) {
				tuple<vector<ResourceRecord>, vector<ResourceRecord>> parentRecords = GetNSGlueRecords(std::get<2>(graph[p[parentIndex]].answer.get()[0]));
				tuple<vector<ResourceRecord>, vector<ResourceRecord>> childRecords = GetNSGlueRecords(std::get<2>(graph[p[parentIndex + 1]].answer.get()[0]));
				CommonSymDiff nsDiff = CompareRRs(std::get<0>(parentRecords), std::get<0>(childRecords));
				CommonSymDiff glueDiff = CompareRRs(std::get<1>(parentRecords), std::get<1>(childRecords));
				if (std::get<1>(nsDiff).size() || std::get<2>(nsDiff).size()) {
					cout << "Delegation Inconsistency in NS records" + QueryFormat(query) + " at " + graph[p[parentIndex]].ns << " and " << graph[p[parentIndex + 1]].ns << endl;
				}
				if (std::get<1>(glueDiff).size() || std::get<2>(glueDiff).size()) {
					cout << "Delegation Inconsistency in Glue records" + QueryFormat(query) + " at " + graph[p[parentIndex]].ns << " and " << graph[p[parentIndex + 1]].ns << endl;
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
					if (i < p.size() - 1 && graph[p[i + 1]].answer) {
						auto& ans = graph[p[i + 1]].answer.get()[0];
						if ((std::get<0>(ans) == ReturnTag::REFUSED || std::get<0>(ans) == ReturnTag::NSNOTFOUND) && query.name == graph[p[i + 1]].query.name) {
							// Child found and also the query matches
							cout << "Lame Inconsistency " + QueryFormat(query) + " at " + graph[p[i]].ns << " and " << graph[p[i + 1]].ns << endl;
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
		std::ofstream ofs;
		ofs.open("hotmail.txt", std::ofstream::out | std::ofstream::app);
		ofs << j.dump(4);
		ofs << "\n";
		ofs.close();
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
		// If the returnTag is a AnsQ and request type contains CNAME then the path ends here for CNAME and this node is a leaf node with respect to t = CNAME.
		if (graph[start].answer && graph[start].answer.get().size() > 0) {
			if (std::get<0>(graph[start].answer.get()[0]) == ReturnTag::ANSQ && query.rrTypes[RRType::CNAME]) {
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
			cout << "loop detected for: " << LabelsToString(query.name) << endl;
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

void CheckPropertiesOnEC(EC& query, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions)
{
	//cout<<QueryFormat(query)<<endl;
	InterpreterGraphWrapper intGraphWrapper;
	BuildInterpretationGraph(query, intGraphWrapper);
	if (num_vertices(intGraphWrapper.intG) == 1) {
		cout << "No NameServer exists to resolve EC- " << QueryFormat(query) << endl;
		return;
	}
	vector<IntpVD> endNodes;
	DFS(intGraphWrapper.intG, intGraphWrapper.startVertex, Path{}, endNodes, pathFunctions);
	//GenerateDotFileInterpreter("Int.dot", intGraphWrapper.intG);
	for (auto f : nodeFunctions) {
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

void WildCardChildEC(std::vector<Label>& childrenLabels, vector<Label>& labels, std::bitset<RRType::N>& typesReq, int index, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {

	EC wildCardMatch;
	wildCardMatch.name.clear();
	for (int i = 0; i < index; i++) {
		wildCardMatch.name.push_back(labels[i]);
	}
	wildCardMatch.rrTypes = typesReq;
	wildCardMatch.excluded = boost::make_optional(std::move(childrenLabels));
	//EC generated
	CheckPropertiesOnEC(wildCardMatch, nodeFunctions, pathFunctions);
}

void NodeEC(LabelGraph& g, vector<Label>& name, std::bitset<RRType::N>& typesReq, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {

	EC present;
	present.name = name;
	present.rrTypes = typesReq;
	//EC generated
	CheckPropertiesOnEC(present, nodeFunctions, pathFunctions);
}

void SubDomainECGeneration(LabelGraph& g, VertexDescriptor start, vector<Label> parentDomainName, std::bitset<RRType::N> typesReq, bool skipLabel, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {

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
			SubDomainECGeneration(g, edge.m_target, name, typesReq, false, nodeFunctions, pathFunctions);
		}
		if (g[edge].type == dname) {
			SubDomainECGeneration(g, edge.m_target, name, typesReq, true, nodeFunctions, pathFunctions);
		}
	}
	g[start].len = beforeLen;
	if (!skipLabel) {
		// EC for the node if the node is not skipped.
		NodeEC(g, name, typesReq, nodeFunctions, pathFunctions);
	}

	// wildcardNode is useful when we want to generate only positive queries and avoid the negations.
	if (wildcardNode) {
		WildCardChildEC(childrenLabels, name, typesReq, name.size(), nodeFunctions, pathFunctions);
	}
	//else {
	//	//Non-existent child category
	//	EC nonExistent;
	//	nonExistent.name = name;
	//	nonExistent.rrTypes.flip();
	//	nonExistent.excluded = boost::make_optional(std::move(childrenLabels));
	//	//EC generated
	//	CheckPropertiesOnEC(nonExistent, nodeFunctions, pathFunctions);
	//}
}

void GenerateECAndCheckProperties(LabelGraph& g, VertexDescriptor root, string userInput, std::bitset<RRType::N> typesReq, bool subdomain, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {
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
				for (auto& encloser : closestEnclosers) {
					SubDomainECGeneration(g, encloser.first, parentDomainName, typesReq, false, nodeFunctions, pathFunctions);
				}
			}
			else {
				NodeEC(g, labels, typesReq, nodeFunctions, pathFunctions);
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
			CheckPropertiesOnEC(nonExistent, nodeFunctions, pathFunctions);
		}
	}
	else {
		cout << "Error: No ClosestEnclosers found(Function: GenerateECAndCheckProperties)" << endl;
		exit(0);
	}

}