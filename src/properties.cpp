#include "interpretation-properties.h"

void interpretation::Graph::Properties::PrettyPrintResponseValue(set<string> response, set<string>& value, const interpretation::Graph& graph, const VertexDescriptor& node, json& tmp)
{
	if (tmp.find("Mismatches") == tmp.end()) {
		tmp["Mismatches"] = {};
	}
	//printf(ANSI_COLOR_RED     "Response Mismatch:"     ANSI_COLOR_RESET "\n");
	json j;
	j["Found"] = std::accumulate(response.begin(), response.end(), std::string{});
	j["Equivalence class"] = graph[node].query.ToString();
	j["At NS"] = graph[node].ns;
	tmp["Mismatches"].push_back(j);
}

tuple<vector<ResourceRecord>, vector<ResourceRecord>> interpretation::Graph::Properties::GetNSGlueRecords(const vector<ResourceRecord>& records)
{
	// From the input set of records, return the NS records and IP records
	vector<ResourceRecord> nsRecords;
	vector<ResourceRecord> glueRecords;
	for (auto r : records) {
		if (r.get_type() == RRType::NS)nsRecords.push_back(r);
		if (r.get_type() == RRType::A || r.get_type() == RRType::AAAA)glueRecords.push_back(r);
	}
	return std::make_tuple(nsRecords, glueRecords);
}

void interpretation::Graph::Properties::CheckResponseReturned(const interpretation::Graph& graph, const vector<VertexDescriptor>& end_nodes, moodycamel::ConcurrentQueue<json>& json_queue, std::bitset<RRType::N> types_req)
{
	/*
	  The set of end nodes is given and checks if some non-empty response is received from all the end nodes for all the requested types.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	*/
	bool incomplete = false;
	for (auto vd : end_nodes) {
		// If the EC is for a non-existent node then by default it will not return an answer
		if (graph[vd].query.nonExistent) {
			return;
		}
		if ((graph[vd].query.rrTypes & types_req).count() > 0 && graph[vd].ns != "") {
			boost::optional<vector<zone::LookUpAnswer>> answer = graph[vd].answer;
			if (answer) {
				if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED || std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {
					incomplete = true;
				}
				else {
					for (auto a : answer.get()) {
						if (!std::get<2>(a).size() && (std::get<1>(a) & types_req).count()) {
							json tmp;
							tmp["Property"] = "ResponseReturned";
							tmp["Equivalence Class"] = graph[vd].query.ToString();
							tmp["Violation"] = "There was no response for T:" + TypeUtils::TypesToString(std::get<1>(a) & types_req) + " at name server:" + graph[vd].ns;
							json_queue.enqueue(tmp);
						}
					}
				}
			}
			else {
				Logger->error(fmt::format("properties.cpp (CheckResponseReturned) - An implementation error - Empty answer at a node for {}", graph[vd].query.ToString()));
			}
		}
	}
}

void interpretation::Graph::Properties::CheckResponseValue(const interpretation::Graph& graph, const vector<VertexDescriptor>& end_nodes, moodycamel::ConcurrentQueue<json>& json_queue, std::bitset<RRType::N> types_requested, set<string> values)
{
	/*
	  The set of end nodes is given and if the return tag is Ans then compare with the user input value.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	*/
	bool foundDiff = false;
	bool incomplete = false;
	json tmp;
	for (auto vd : end_nodes) {
		if ((graph[vd].query.rrTypes & types_requested).count() > 0) {
			if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::ANS || std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REWRITE) {
				set<string> rdatas;
				for (auto rr : std::get<2>(graph[vd].answer.get()[0])) {
					if (types_requested[rr.get_type()] == 1) {
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
		json_queue.enqueue(tmp);
	}
}

void interpretation::Graph::Properties::CheckSameResponseReturned(const interpretation::Graph& graph, const vector<VertexDescriptor>& end_nodes, moodycamel::ConcurrentQueue<json>& json_queue, std::bitset<RRType::N> typesReq)
{
	/*
	  The set of end nodes is given and checks if same response is received from all the end nodes.
	  We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
	  The end nodes might be a mix of CNAME node and others.
	*/
	boost::optional<vector<zone::LookUpAnswer>> response;
	bool incomplete = false;
	bool foundDiff = false;
	vector<VertexDescriptor> cname_endnodes{}; //TODO: Check for CNAME End nodes separately.
	for (auto vd : end_nodes) {
		if ((graph[vd].query.rrTypes & typesReq).count() > 0 && graph[vd].ns != "") {
			boost::optional<vector<zone::LookUpAnswer>> answer = graph[vd].answer;
			if (answer) {
				if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED || std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {
					incomplete = true;
				}
				else if (std::get<0>(answer.get()[0]) == ReturnTag::REWRITE) {
					cname_endnodes.push_back(vd);
				}
				else if (!response) {
					response = answer;
				}
				else {
					CommonSymDiff diff = RRUtils::CompareRRs(std::get<2>(answer.get()[0]), std::get<2>(response.get()[0]));
					if (std::get<1>(diff).size() || std::get<2>(diff).size()) {
						foundDiff = true;
					}
				}
			}
			else {
				Logger->error(fmt::format("properties.cpp (CheckSameResponseReturned) - An implementation error - Empty answer at a node for {}", graph[vd].query.ToString()));
			}
		}
	}
	if (foundDiff) {
		json tmp;
		tmp["Property"] = "ResponseConsistency";
		tmp["Equivalence Class"] = graph[end_nodes[0]].query.ToString();
		tmp["Violation"] = "Difference in responses found";
		json_queue.enqueue(tmp);
	}
}

void interpretation::Graph::Properties::CheckDelegationConsistency(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue)
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
				CommonSymDiff nsDiff = RRUtils::CompareRRs(std::get<0>(parentRecords), std::get<0>(childRecords));
				CommonSymDiff glueDiff = RRUtils::CompareRRs(std::get<1>(parentRecords), std::get<1>(childRecords));
				if (std::get<1>(nsDiff).size() || std::get<2>(nsDiff).size()) {
					json tmp;
					tmp["Property"] = "Delegation Consistency";
					tmp["Equivalence Class"] = query.ToString();
					tmp["Violation"] = "Inconsistency in NS records  at " + graph[p[parentIndex]].ns + " and " + graph[p[static_cast<long long>(parentIndex) + 1]].ns;
					json_queue.enqueue(tmp);
				}
				if (std::get<1>(glueDiff).size() || std::get<2>(glueDiff).size()) {
					json tmp;
					tmp["Property"] = "Delegation Consistency";
					tmp["Equivalence Class"] = query.ToString();
					tmp["Violation"] = "Inconsistency in Glue records  at " + graph[p[parentIndex]].ns + " and " + graph[p[static_cast<long long>(parentIndex) + 1]].ns;
					json_queue.enqueue(tmp);
				}
			}
		}
		else {
			Logger->error(fmt::format("properties.cpp (CheckDelegationConsistency) - An implementation error - The path does not start at a dummy node for {}", graph[p[0]].query.ToString()));
		}
	}
}

void interpretation::Graph::Properties::CheckLameDelegation(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue)
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
							tmp["Equivalence Class"] = query.ToString();
							tmp["Violation"] = "Inconsistency at " + graph[p[i]].ns + " and " + graph[p[static_cast<long long>(i) + 1]].ns;
							json_queue.enqueue(tmp);
						}
					}
				}
			}
		}
		else {
			Logger->error(fmt::format("properties.cpp (CheckLameDelegation) - An implementation error - The path does not start at a dummy node for {}", graph[p[0]].query.ToString()));
		}
	}
}

void interpretation::Graph::Properties::NameServerContact(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue, vector<vector<NodeLabel>> allowed_domains)
{
	/*
	 At any point in the resolution process, the query should not be sent to a name server outside the domain.
	*/
	for (int i = 0; i < p.size(); i++) {
		if (graph[p[i]].ns != "") {
			if (!LabelUtils::SubDomainCheck(allowed_domains, LabelUtils::StringToLabels(graph[p[i]].ns))) {
				json tmp;
				tmp["Property"] = "Name Server Contact";
				tmp["Equivalence Class"] = graph[p[i]].query.ToString();
				tmp["Violation"] = "Query is sent to NS:" + graph[p[i]].ns + " which is not under " + LabelUtils::LabelsToString(allowed_domains);
				json_queue.enqueue(tmp);
			}
		}
	}
}

void interpretation::Graph::Properties::NumberOfHops(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue, int num_hops)
{
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
		tmp["Equivalence Class"] = graph[p[0]].query.ToString();
		std::ostringstream stringStream;
		stringStream << "Number of hops (" << nameServers.size() << ") exceeded " << num_hops;
		tmp["Violation"] = stringStream.str();
		json_queue.enqueue(tmp);
	}
}

void interpretation::Graph::Properties::NumberOfRewrites(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue, int num_rewrites)
{
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
		tmp["Equivalence Class"] = graph[p[0]].query.ToString();
		std::ostringstream stringStream;
		stringStream << "Number of rewrites (" << rewrites << ") exceeded " << num_rewrites;
		tmp["Violation"] = stringStream.str();
		json_queue.enqueue(tmp);
	}
}

void interpretation::Graph::Properties::QueryRewrite(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue, vector<vector<NodeLabel>> domain)
{
	/*
	  If there is a node with answer tag as AnsQ then the new query should be under the subdomain of domain.
	*/
	for (int i = 0; i < p.size(); i++) {
		if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
			// Rewrite happened at this node
			//The answer would have a DNAME/CNAME Resource record and the new query would be availble in the next node in the path.
			if (i < p.size() - 1) {
				if (!LabelUtils::SubDomainCheck(domain, graph[p[static_cast<long long>(i) + 1]].query.name)) {
					json tmp;
					tmp["Property"] = "Query Rewrite";
					tmp["Equivalence Class"] = graph[p[i]].query.ToString();
					tmp["Violation"] = "Query is rewritten to " + graph[p[static_cast<long long>(i) + 1]].query.ToString() + " at NS:" + graph[p[i]].ns + " which is not under any of " + LabelUtils::LabelsToString(domain);
					json_queue.enqueue(tmp);
				}
			}
			else {
				// Rewrite can be the last node in the path if the user requested CNAME type.
				// Logger->error(fmt::format("properties.cpp (QueryRewrite) - An implementation error - REWRITE is the last node in the path: {}", graph[p[0]].query.ToString()));
			}
		}
	}
}

void interpretation::Graph::Properties::RewriteBlackholing(const interpretation::Graph& graph, const Path& p, moodycamel::ConcurrentQueue<json>& json_queue)
{
	/*
	  If there is a node with answer tag as REWRITE then the last node in the path should not be an NXDOMAIN. (REFUSED/NSNOTFOUND)
	  If the last node is NXDOMAIN then it means the initial query is rewritten and the new query doesn't have resource record of any type.
	  We may flag things which end up with REFUSED/NSNOTFOUND to be conservative.
	*/
	for (int i = 0; i < p.size(); i++) {
		if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
			// Rewrite happened at this node
			if (i < p.size() - 1) {
				auto& end_tag = std::get<0>(graph[p[p.size() - 1]].answer.get()[0]);
				if (end_tag == ReturnTag::NX) {
					json tmp;
					tmp["Property"] = "Rewrite Blackholing";
					tmp["Equivalence Class"] = graph[p[i]].query.ToString();
					tmp["Violation"] = "Query is eventually rewritten to " + graph[p[p.size() - 1]].query.ToString() + " for which there doesn't exist any resource record of any type";
					json_queue.enqueue(tmp);
				}
				else if (end_tag == ReturnTag::REFUSED || end_tag == ReturnTag::NSNOTFOUND) {
					json tmp;
					tmp["Property"] = "Rewrite Blackholing";
					tmp["Equivalence Class"] = graph[p[i]].query.ToString();
					tmp["Violation"] = "Query is eventually rewritten to " + graph[p[p.size() - 1]].query.ToString() + " for which there is no sufficient information at NS: " + graph[p[p.size() - 1]].ns + " to decide whether it is blackholed";
					json_queue.enqueue(tmp);
				}
			}
			else {
				// Rewrite can be the last node in the path if the user requested CNAME type.
				//Logger->error(fmt::format("properties.cpp (RewriteBlackholing) - An implementation error - REWRITE is the last node in the path: {}", graph[p[0]].query.ToString()));
			}
			break;
		}
	}
}
