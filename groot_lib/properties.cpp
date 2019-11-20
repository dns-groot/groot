#include "properties.h"
#include <numeric>
#include <utility>

void CheckResponseReturned(const InterpreterGraph& graph, const vector<InterpreterVertexDescriptor>& endNodes, std::bitset<RRType::N> typesReq) {
	for (auto v : endNodes) {
		if ((graph[v].query.rrTypes & typesReq).count() > 0) {
			boost::optional<vector<ResourceRecord>> answer = graph[v].answer;
			if (answer && answer.get().size()) {
				//Path returns some response
			}
			else {
				if (graph[v].query.excluded) {
					cout << "There was no response for Q: ~{ }.";
				}
				else {
					cout << "There was no response for Q: ";
				}
				cout << LabelsToString(graph[v].query.name) << " for T:" << RRTypesToString(graph[v].query.rrTypes & typesReq);
				cout << " at name server: " << graph[v].ns << endl;
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

void CheckSameResponseReturned(const InterpreterGraph& graph, const vector<InterpreterVertexDescriptor>& endNodes, std::bitset<RRType::N> typesReq) {
	boost::optional<vector<ResourceRecord>> response;
	bool noResponse = false;
	for (auto v : endNodes) {
		if ((graph[v].query.rrTypes & typesReq).count() > 0) {
			boost::optional<vector<ResourceRecord>> answer = graph[v].answer;
			if (answer) {
				if (!response) {
					response = answer;
				}
				else {
					//Compare responses
					std::bitset<RRType::N> typesDiff = CompareResponse(response.get(), answer.get(), typesReq);
					if (typesDiff.count() > 0) {
						if (graph[v].query.excluded) {
							cout << "Difference in responses found for Q: ~{ }.";
						}
						else {
							cout << "Difference in responses found for Q: ";
						}
						cout << LabelsToString(graph[v].query.name) << " for T:" << RRTypesToString(typesDiff) << endl;
						break;
					}
				}
			}
			else {
				noResponse = true;
			}
		}
	}
}

void PrettyPrintResponseValue(vector<string>& response, vector<string>& value, const InterpreterGraph& graph, const InterpreterVertexDescriptor& node) {

	std::string s = std::accumulate(value.begin(), value.end(), std::string{});
	//printf(ANSI_COLOR_RED     "Response Mismatch:"     ANSI_COLOR_RESET "\n");
	cout << "Response Mismatch:" << endl;
	cout << "\t Expected: " << s <<endl;
	cout << "\t Found: " << std::accumulate(response.begin(), response.end(), std::string{}) << endl;;
	cout << "\t At NS: " << graph[node].ns<<endl;
	cout << "\t" << QueryFormat(graph[node].query) << endl;
}

void CheckResponseValue(const InterpreterGraph& graph, const vector<InterpreterVertexDescriptor>& endNodes, std::bitset<RRType::N> typesReq, vector<string> value) {

	bool foundDiff = false;
	for (auto v : endNodes) {
		if ((graph[v].query.rrTypes & typesReq).count() > 0) {
			boost::optional<vector<ResourceRecord>> answer = graph[v].answer;
			vector<string> rdatas;
			if (answer) {
				bool foundMatching = false;
				
				for (auto rr : answer.get()) {
					if (typesReq[rr.get_type()] == 1) {
						rdatas.push_back(rr.get_rdata());
					}
				}
				if (rdatas.size() == value.size()) {
					foundMatching  = std::equal(rdatas.begin(), rdatas.end(), value.begin());
				}
				else {
					foundMatching = false;
				}
				if (!foundMatching) {
					PrettyPrintResponseValue(rdatas, value, graph, v);
				}
				foundDiff = foundMatching == false ? true : foundDiff;
			}
			else {
				if (value.size() != 0) {
					PrettyPrintResponseValue(rdatas, value, graph, v);
				}
			}
		}
	}
}

void NumberOfRewrites(const InterpreterGraph& graph, const Path& p, int num_rewrites) {
	int rewrites = 0;
	for (int i = 0; i < p.size() - 1; i++) {
		if (graph[p[i]].query.name != graph[p[i + 1]].query.name) {
			rewrites++;
		}
	}
	if (rewrites > num_rewrites) {
		cout << " Number of rewrites exceeded " << num_rewrites;
		if (graph[p[0]].query.excluded) {
			cout << " for Q: ~{ }.";
		}
		else {
			cout << " for Q: ";
		}
	}
}

void NumberOfHops(const InterpreterGraph& graph, const Path& p, int num_hops) {
	int hops = p.size() - 1;
	if (hops > num_hops) {
		cout << " Number of hops exceeded " << num_hops;
		if (graph[p[0]].query.excluded) {
			cout << " for Q: ~{ }.";
		}
		else {
			cout << " for Q: ";
		}
		//cout << graph[p[0]].query.name << " for T:" << RRTypesToString(graph[p[0]].query.rrTypes)<<endl;
	}
}

string QueryFormat(const EC& query) {
	string q = "";
	if (query.excluded) {
		q+=" for Q: ~{ }.";
	}
	else {
		q+= " for Q: ";
	}
	q += LabelsToString(query.name);
	return q;
}

void CheckDelegationConsistency(const InterpreterGraph& graph, const Path& p)
{
	//The path should have at least three nodes including the dummy node.
	if (p.size() > 2) {
		//TODO: Parent and Child should be answering for the user input query
		InterpreterVertexDescriptor lastNode = p.back();
		InterpreterVertexDescriptor parentNode = p[p.size() - 2];
		if (graph[lastNode].answer && !graph[parentNode].answer) {
			cout << "Delegation Inconsistency " + QueryFormat(graph[lastNode].query)+ " at " + graph[parentNode].ns<< " and " << graph[lastNode].ns << endl ;
		}
		else if (!graph[lastNode].answer && graph[parentNode].answer) {
			cout << "Delegation Inconsistency " + QueryFormat(graph[lastNode].query) + " at " + graph[parentNode].ns << " and " << graph[lastNode].ns << endl;
		}
		else {
			vector<ResourceRecord> nodeGlueRecords = SeparateGlueRecords(graph[lastNode].answer.get());
			vector<ResourceRecord> parentGlueRecords = SeparateGlueRecords(graph[parentNode].answer.get());
			std::bitset<RRType::N> typesReq;
			typesReq.set(RRType::A);
			typesReq.set(RRType::AAAA);
			/*if (CompareResponse(nodeGlueRecords, parentGlueRecords, typesReq).count() > 0) {
				cout << "Delegation Inconsistency " + QueryFormat(graph[lastNode].query) + " at " + graph[parentNode].ns << " and " << graph[lastNode].ns << endl;
				return;
			}*/
			typesReq.reset(RRType::A);
			typesReq.reset(RRType::AAAA);
			typesReq.set(RRType::NS);
			if (CompareResponse(graph[lastNode].answer.get(), graph[parentNode].answer.get(), typesReq).count() > 0) {
				cout << "Delegation Inconsistency " + QueryFormat(graph[lastNode].query) + " at " + graph[parentNode].ns << " and " << graph[lastNode].ns << endl;
				return;
			}
		}
	}
}

void CheckLameDelegation(const InterpreterGraph& graph, const Path& p)
{
	//The final node should have an SOA?
}



void DFS(InterpreterGraph& graph, InterpreterVertexDescriptor start, Path p, vector<InterpreterVertexDescriptor>& endNodes, vector<std::function<void(const InterpreterGraph&, const Path&)>>& pathFunctions) {
	EC& query = graph[start].query;
	if (graph[start].ns != "") {
		//If the response is a CNAME and request type contains CNAME then it is resolved in this step
		if (graph[start].answer && graph[start].answer.get().size()>0 && graph[start].answer.get()[0].get_type() == RRType::CNAME && query.rrTypes[RRType::CNAME]) {
			if (endNodes.end() == std::find(endNodes.begin(), endNodes.end(), start)) {
				endNodes.push_back(start);
			}
			// A path is seen only once
			for (auto& f : pathFunctions) {
				f(graph, p);
			}
		}
	}
	for (auto v : p) {
		if (v == start) {
			//cout << "loop detected for: " << query.name << endl;
			return;
		}
	}
	p.push_back(start);
	for (InterpreterEdgeDescriptor edge : boost::make_iterator_range(out_edges(start, graph))) {
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

void CheckPropertiesOnEC(EC& query, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions)
{	
	//cout<<QueryFormat(query)<<endl;
	InterpreterGraphWrapper intGraphWrapper;
	BuildInterpretationGraph(query, intGraphWrapper);
	vector<InterpreterVertexDescriptor> endNodes;
	DFS(intGraphWrapper.intG, intGraphWrapper.startVertex, Path{}, endNodes, pathFunctions);
	GenerateDotFileInterpreter("Int.dot", intGraphWrapper.intG);
	for (auto f : nodeFunctions) {
		f(intGraphWrapper.intG, endNodes);
	}
}


vector<closestNode> SearchNode(LabelGraph& g, VertexDescriptor closestEncloser, vector<Label>& labels, int index) {
	/*
		Given a user input the functions returns all the closest enclosers alng with the number of labels matched.
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

void WildCardChildEC(std::vector<Label>& childrenLabels, vector<Label>& labels, std::bitset<RRType::N>& typesReq, int index, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {

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

void NodeEC(LabelGraph& g, VertexDescriptor& node, vector<Label>& name, std::bitset<RRType::N>& typesReq, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {

	EC present;
	present.name = name;
	present.rrTypes = typesReq;
	//EC generated
	CheckPropertiesOnEC(present, nodeFunctions, pathFunctions);
}

void SubDomainECGeneration(LabelGraph& g, VertexDescriptor start, vector<Label> parentDomainName, std::bitset<RRType::N> typesReq, bool skipLabel, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {

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
			else {
				childrenLabels.push_back(g[edge.m_target].name);
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
		NodeEC(g, start, name, typesReq, nodeFunctions, pathFunctions);
	}
	if (wildcardNode) {
		WildCardChildEC(childrenLabels, name, typesReq, name.size(), nodeFunctions, pathFunctions);
	}
	else {
		//Non-existent child category
		EC nonExistent;
		nonExistent.name = name;
		nonExistent.rrTypes.flip();
		nonExistent.excluded = boost::make_optional(std::move(childrenLabels));
		//EC generated
		CheckPropertiesOnEC(nonExistent, nodeFunctions, pathFunctions);
	}
}

void GenerateECAndCheckProperties(LabelGraph& g, VertexDescriptor root, string userInput, std::bitset<RRType::N> typesReq, bool subdomain, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions) {
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
				NodeEC(g, closestEnclosers[0].first, labels, typesReq, nodeFunctions, pathFunctions);
			}
		}
		else {
			//Sub-domain queries can not be peformed.
			if (subdomain) {
				cout << "The complete domain: " << userInput << " doesn't exist so sub-domain is not valid";
			}
			std::vector<Label> childrenLabels;
			std::optional<VertexDescriptor>  wildcardNode;

			for (auto& encloser : closestEnclosers) {
				for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(encloser.first, g))) {
					if (g[edge].type == normal) {
						if (g[edge.m_target].name.get() == "*") {
							wildcardNode = edge.m_target;
						}
						else {
							childrenLabels.push_back(g[edge.m_target].name);
						}
					}
				}
			}
			// The query might match a wildcard or its part of non-existent child nodes.
			EC nonExistent;
			nonExistent.name.clear();
			for (int i = 0; i < matchedIndex; i++) {
				nonExistent.name.push_back(labels[i]);
			}
			nonExistent.rrTypes = typesReq;
			nonExistent.excluded = boost::make_optional(std::move(childrenLabels));
			//EC generated
			CheckPropertiesOnEC(nonExistent, nodeFunctions, pathFunctions);
		}
	}
	else {
		cout << "Error: No ClosestEnclosers found(Function: GenerateECAndCheckProperties)" << endl;
		exit(0);
	}
	
}