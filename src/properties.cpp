#include "interpretation-properties.h"
#include <boost/filesystem.hpp>

void interpretation::Graph::Properties::PrettyPrintResponseValue(
    set<string> response,
    set<string> &value,
    const interpretation::Graph &graph,
    const VertexDescriptor &node,
    json &tmp)
{
    if (tmp.find("Mismatches") == tmp.end()) {
        tmp["Mismatches"] = {};
    }
    // printf(ANSI_COLOR_RED     "Response Mismatch:"     ANSI_COLOR_RESET "\n");
    json j;
    j["Found"] = std::accumulate(response.begin(), response.end(), std::string{});
    j["Query"] = graph[node].query.ToString();
    j["Nameserver"] = graph[node].ns;
    tmp["Mismatches"].push_back(j);
}

tuple<vector<ResourceRecord>, vector<ResourceRecord>> interpretation::Graph::Properties::GetNSGlueRecords(
    const vector<ResourceRecord> &records)
{
    // From the input set of records, return the NS records and IP records
    vector<ResourceRecord> nsRecords;
    vector<ResourceRecord> glueRecords;
    for (auto r : records) {
        if (r.get_type() == RRType::NS)
            nsRecords.push_back(r);
        if (r.get_type() == RRType::A || r.get_type() == RRType::AAAA)
            glueRecords.push_back(r);
    }
    return std::make_tuple(nsRecords, glueRecords);
}

void interpretation::Graph::Properties::CheckResponseReturned(
    const interpretation::Graph &graph,
    const vector<VertexDescriptor> &end_nodes,
    moodycamel::ConcurrentQueue<json> &json_queue,
    std::bitset<RRType::N> types_req)
{
    /*
      The set of end nodes is given and checks if some non-empty response is received from all the end nodes for all the
      requested types. We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
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
                if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED ||
                    std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {
                    incomplete = true;
                } else {
                    for (auto a : answer.get()) {
                        if (!std::get<2>(a).size() && (std::get<1>(a) & types_req).count()) {
                            json tmp;
                            tmp["Property"] = "ResponseReturned";
                            tmp["Query"] = graph[vd].query.ToString();
                            tmp["Violation"]["Types"] = TypeUtils::TypesToString(std::get<1>(a) & types_req);
                            tmp["Violation"]["Nameserver"] = graph[vd].ns;
                            json_queue.enqueue(tmp);
                        }
                    }
                }
            } else {
                Logger->error(fmt::format(
                    "properties.cpp (CheckResponseReturned) - An implementation error - Empty answer at a node for {}",
                    graph[vd].query.ToString()));
            }
        }
    }
}

void interpretation::Graph::Properties::CheckResponseValue(
    const interpretation::Graph &graph,
    const vector<VertexDescriptor> &end_nodes,
    moodycamel::ConcurrentQueue<json> &json_queue,
    std::bitset<RRType::N> types_requested,
    set<string> values)
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
            if (std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::ANS ||
                std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REWRITE) {
                set<string> rdatas;
                for (auto rr : std::get<2>(graph[vd].answer.get()[0])) {
                    if (types_requested[rr.get_type()] == 1) {
                        rdatas.insert(rr.get_rdata());
                    }
                }
                if (rdatas != values) {
                    PrettyPrintResponseValue(rdatas, values, graph, vd, tmp);
                }
            } else if (
                std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NX ||
                std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::YX) {
                PrettyPrintResponseValue(set<string>{}, values, graph, vd, tmp);
            } else if (
                std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
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

void interpretation::Graph::Properties::CheckSameResponseReturned(
    const interpretation::Graph &graph,
    const vector<VertexDescriptor> &end_nodes,
    moodycamel::ConcurrentQueue<json> &json_queue,
    std::bitset<RRType::N> typesReq)
{
    /*
      The set of end nodes is given and checks if same response is received from all the end nodes.
      We may encounter nodes with Refused/NSnotfound in which case we have incomplete information.
      The end nodes might be a mix of CNAME node and others.
    */
    boost::optional<vector<zone::LookUpAnswer>> response;
    bool incomplete = false;
    bool foundDiff = false;
    /*
       CNAME end nodes - (1) Nodes with Rewrite tag in this vector
                         (2) Nodes with the query requesting for CNAME (covers 1 too)
    */
    vector<VertexDescriptor> cname_end_nodes{};
    for (auto vd : end_nodes) {
        if ((graph[vd].query.rrTypes & typesReq).count() > 0 && graph[vd].ns != "") {
            boost::optional<vector<zone::LookUpAnswer>> answer = graph[vd].answer;
            if (answer) {
                if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED ||
                    std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {
                    incomplete = true;
                } else {
                    if (graph[vd].query.rrTypes[RRType::CNAME]) {
                        cname_end_nodes.push_back(vd);
                    }
                    if (!response) {
                        response = answer;
                    } else {
                        CommonSymDiff diff =
                            RRUtils::CompareRRs(std::get<2>(answer.get()[0]), std::get<2>(response.get()[0]));
                        if (std::get<1>(diff).size() || std::get<2>(diff).size()) {
                            foundDiff = true;
                        }
                    }
                }
            } else {
                Logger->error(fmt::format(
                    "properties.cpp (CheckSameResponseReturned) - An implementation error - "
                    "Empty answer at a node for {}",
                    graph[vd].query.ToString()));
            }
        }
    }
    response = {};
    for (auto &vd : cname_end_nodes) {
        if (!response) {
            response = graph[vd].answer;
        } else {
            if (std::get<0>(graph[vd].answer.get()[0]) != std::get<0>(response.get()[0])) {
                foundDiff = true;
            } else {
                // If only CNAME is requested then the answer will only have one element
                // If CNAME and other types are requested then there might be a node with Rewrite
                CommonSymDiff diff =
                    RRUtils::CompareRRs(std::get<2>(graph[vd].answer.get()[0]), std::get<2>(response.get()[0]));
                if (std::get<1>(diff).size() || std::get<2>(diff).size()) {
                    foundDiff = true;
                }
            }
        }
    }
    // TODO: Provide more info about the inconsistency.
    if (foundDiff) {
        json tmp;
        tmp["Property"] = "Response Consistency";
        tmp["Query"] = graph[end_nodes[0]].query.ToString();
        json_queue.enqueue(tmp);
    }
}

void interpretation::Graph::Properties::ZeroTTL(
    const interpretation::Graph &graph,
    const vector<VertexDescriptor> &end_nodes,
    moodycamel::ConcurrentQueue<json> &json_queue,
    std::bitset<RRType::N> typesReq)
{
    /*
      The set of end nodes is given and checks if the response contains resource records with zero TTL
      for the given types.
    */
    for (auto &vd : end_nodes) {
        if ((graph[vd].query.rrTypes & typesReq).count() > 0 && graph[vd].ns != "") {
            boost::optional<vector<zone::LookUpAnswer>> answer = graph[vd].answer;
            if (answer) {
                if (std::get<0>(answer.get()[0]) == ReturnTag::REFUSED ||
                    std::get<0>(answer.get()[0]) == ReturnTag::NSNOTFOUND) {

                } else {
                    auto &rrs = std::get<2>(answer.get()[0]);
                    for (auto &r : rrs) {
                        if (r.get_ttl() == 0) {
                            json tmp;
                            tmp["Property"] = "Zero TTL";
                            tmp["Query"] = graph[end_nodes[0]].query.ToString();
                            tmp["Violation"]["Nameserver"] = graph[vd].ns;
                            tmp["Violation"]["Record"] = r.toString();
                            json_queue.enqueue(tmp);
                        }
                    }
                }
            } else {
                Logger->error(fmt::format(
                    "properties.cpp (ZeroTTL) - An implementation error - "
                    "Empty answer at a node for {}",
                    graph[vd].query.ToString()));
            }
        }
    }
}

void interpretation::Graph::Properties::AllAliases(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue,
    vector<vector<NodeLabel>> canonical_names)
{
    /*
      If there is a node with answer tag as REWRITE then there should be a node with query name in the input
    */
    for (int i = 0; i < p.size(); i++) {
        if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
            // Rewrite happened at this node and it can be CNAME or DNAME
            for (int j = i + 1; j < p.size(); j++) {
                auto it = std::find(canonical_names.begin(), canonical_names.end(), graph[p[j]].query.name);
                // The query after rewrite should not be have an excluded set as it implies it is a DNAME rewrite and we
                // don't check it in the using the find function.
                if (it != canonical_names.end() && !graph[p[j]].query.excluded) {
                    json tmp;
                    tmp["Property"] = "All Aliases";
                    tmp["Query"] = graph[p[i]].query.ToString();
                    tmp["Canonical Name"] = graph[p[j]].query.ToString();
                    json_queue.enqueue(tmp);
                }
            }
        }
    }
}

void interpretation::Graph::Properties::InfiniteDName(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue)
{
    std::unordered_map<const ResourceRecord, tuple<uint32_t, string>, rrHash> DMap;
    /*
      If there is a node with answer tag as REWRITE then there should be a node with query name in the input
    */
    for (int i = 0; i < p.size(); i++) {
        if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
            // Rewrite happened at this node and it can be CNAME or DNAME
            vector<zone::LookUpAnswer> answers = graph[p[i]].answer.get();
            for (zone::LookUpAnswer ans : answers) {
                auto RRList = std::get<2>(ans);
                for (const ResourceRecord rr : RRList) {
                    if (DMap.find(rr) != DMap.end()) {
                        if (graph[p[i]].query.name.size() >= std::get<0>(DMap[rr])) {
                            json tmp;
                            tmp["Property"] = "Infinite DName Recursion";
                            tmp["Query1"] = std::get<1>(DMap[rr]);
                            tmp["Query2"] = graph[p[i]].query.ToString();
                            int j = 1;
                            for (const auto &[key, value] : DMap) {
                                ResourceRecord r = key;
                                string seq = "RR" + to_string(j);
                                string entry = r.toString();
                                tmp["RRsUsed"][seq] = entry;
                                j++;
                            }
                            json_queue.enqueue(tmp);
                            return;
                        }
                        uint32_t size = graph[p[i]].query.name.size();
                        string name = graph[p[i]].query.ToString();
                        DMap[rr] = std::make_tuple(size, name);
                    }
                    uint32_t size = graph[p[i]].query.name.size();
                    string name = graph[p[i]].query.ToString();
                    tuple<uint32_t, string> tups = std::make_tuple(size, name);
                    DMap.insert(std::pair < ResourceRecord, tuple<uint32_t, string>>(rr, tups));
                }
            }
        }
    }
}

void interpretation::Graph::Properties::CheckDelegationConsistency(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue)
{
    /*
      The path should have at least three nodes including the dummy node.
      There should be consective nodes answering for the given query, more specifically, the parent should have Ref type
      and the child with Ans type. If there is no such pair then the given query name is not a zone cut and delegation
      consistency is not a valid property to check. Nameserver shadowing case: Consider the query 511.dabomb.com.ar
      which is sent to ns1.com.ar. which provides Ref to dabomb.com.ar. as server X. The server X has zone files for 511
      and dabomb and picks the 511 one to answer the query(Ans). Both the parent and child should be answering for the
      same domain_name.
    */
    if (p.size() > 2) {
        if (graph[p[0]].ns == "") {
            const EC &query = graph[p[0]].query;
            int parentIndex = -1;
            for (int i = 0; i < p.size(); i++) {
                if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REF) {
                    // Parent found
                    if (i < p.size() - 1 && graph[p[static_cast<long long>(i) + 1]].answer) {
                        auto &ans = graph[p[static_cast<long long>(i) + 1]].answer.get()[0];
                        if (std::get<0>(ans) == ReturnTag::ANS && std::get<1>(ans)[RRType::NS] &&
                            std::get<2>(ans).size() > 0 &&
                            query.name == graph[p[static_cast<long long>(i) + 1]].query.name) {
                            // Child found (should have at least one NS record) and also the query matches
                            parentIndex = i;
                        }
                    }
                }
            }
            if (parentIndex != -1) {
                tuple<vector<ResourceRecord>, vector<ResourceRecord>> parent_records =
                    GetNSGlueRecords(std::get<2>(graph[p[parentIndex]].answer.get()[0]));
                tuple<vector<ResourceRecord>, vector<ResourceRecord>> child_records =
                    GetNSGlueRecords(std::get<2>(graph[p[static_cast<long long>(parentIndex) + 1]].answer.get()[0]));

                // Parent and child be about the same domain name
                if (std::get<0>(parent_records).size() && std::get<0>(child_records).size()) {
                    if (std::get<0>(parent_records)[0].get_name() != std::get<0>(child_records)[0].get_name()) {
                        return;
                    }
                }

                // Remove glue records that are required by parent but not child
                if (std::get<0>(child_records).size()) {
                    vector<NodeLabel> child_domain = std::get<0>(child_records)[0].get_name();
                    vector<ResourceRecord> &parent_glue_records = std::get<1>(parent_records);
                    for (auto it = parent_glue_records.begin(); it != parent_glue_records.end();) {
                        if (!LabelUtils::SubDomainCheck(child_domain, it->get_name())) {
                            it = parent_glue_records.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                CommonSymDiff ns_diff = RRUtils::CompareRRs(std::get<0>(parent_records), std::get<0>(child_records));
                CommonSymDiff glue_diff = RRUtils::CompareRRs(std::get<1>(parent_records), std::get<1>(child_records));
                if (std::get<1>(ns_diff).size() || std::get<2>(ns_diff).size()) {
                    json tmp;
                    tmp["Property"] = "Delegation Consistency";
                    tmp["Query"] = query.ToString();
                    tmp["Violation"]["Nameserver1"] = graph[p[parentIndex]].ns;
                    tmp["Violation"]["Nameserver2"] = graph[p[static_cast<long long>(parentIndex) + 1]].ns;
                    tmp["Violation"]["InconsistencyType"] = "NS";
                    json_queue.enqueue(tmp);
                }
                if (std::get<1>(glue_diff).size() || std::get<2>(glue_diff).size()) {
                    json tmp;
                    tmp["Property"] = "Delegation Consistency";
                    tmp["Query"] = query.ToString();
                    tmp["Violation"]["Nameserver1"] = graph[p[parentIndex]].ns;
                    tmp["Violation"]["Nameserver2"] = graph[p[static_cast<long long>(parentIndex) + 1]].ns;
                    tmp["Violation"]["InconsistencyType"] = "Glue";
                    json_queue.enqueue(tmp);
                }
            }
        } else {
            Logger->error(fmt::format(
                "properties.cpp (CheckDelegationConsistency) - An implementation error - The "
                "path does not start at a dummy node for {}",
                graph[p[0]].query.ToString()));
        }
    }
}

void interpretation::Graph::Properties::CheckLameDelegation(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue)
{
    /*
      The path should have at least three nodes including the dummy node.
      There should be consective nodes answering for the given query, more specifically, the parent should have Ref type
      and the child with Refused/NSnotfound type. If there is such a pair then from the given zone fileswe flag it as
      lame delegation which may be a false positive if we didn't have access to all zone files at the child NS.
    */
    if (p.size() > 2) {
        if (graph[p[0]].ns == "") {
            const EC &query = graph[p[0]].query;
            int parentIndex = -1;
            for (int i = 0; i < p.size(); i++) {
                if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REF) {
                    // Parent found
                    if (i < p.size() - 1 && graph[p[static_cast<long long>(i) + 1]].answer) {
                        auto &ans = graph[p[static_cast<long long>(i) + 1]].answer.get()[0];
                        if ((std::get<0>(ans) == ReturnTag::REFUSED) &&
                            query.name == graph[p[static_cast<long long>(i) + 1]].query.name) {
                            // if ((std::get<0>(ans) == ReturnTag::REFUSED || std::get<0>(ans) == ReturnTag::NSNOTFOUND)
                            // && query.name == graph[p[static_cast<long long>(i) + 1]].query.name) {
                            // Child found and also the query matches
                            json tmp;
                            tmp["Property"] = "Lame Delegation";
                            tmp["Query"] = query.ToString();
                            tmp["Violation"]["Nameserver1"] = graph[p[i]].ns;
                            tmp["Violation"]["Nameserver2"] = graph[p[static_cast<long long>(i) + 1]].ns;
                            json_queue.enqueue(tmp);
                        }
                    }
                }
            }
        } else {
            Logger->error(fmt::format(
                "properties.cpp (CheckLameDelegation) - An implementation error - The path does "
                "not start at a dummy node for {}",
                graph[p[0]].query.ToString()));
        }
    }
}

void interpretation::Graph::Properties::DNAMESubstitutionExceedesLength(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue)
{
    auto &ans = graph[p[p.size() - 1]].answer;
    if (ans) {
        if (std::get<0>(ans.get()[0]) == ReturnTag::YX) {
            json tmp;
            tmp["Property"] = "DNAME Substitution exceeds length";
            tmp["Query"] = graph[p[0]].query.ToString();
            tmp["Violation"]["RewriteTarget"] = graph[p[p.size() - 1]].query.ToString();
            tmp["Violation"]["Nameserver"] = graph[p[p.size() - 1]].ns;
            json_queue.enqueue(tmp);
        }
    }
}

void interpretation::Graph::Properties::NameServerContact(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue,
    vector<vector<NodeLabel>> allowed_domains)
{
    /*
     At any point in the resolution process, the query should not be sent to a name server outside the domain.
    */
    for (int i = 0; i < p.size(); i++) {
        if (graph[p[i]].ns != "") {
            if (!LabelUtils::SubDomainCheck(allowed_domains, LabelUtils::StringToLabels(graph[p[i]].ns))) {
                json tmp;
                tmp["Property"] = "Name Server Contact";
                tmp["Query"] = graph[p[i]].query.ToString();
                tmp["Violation"]["ExternalNameserver"] = graph[p[i]].ns;
                tmp["Violation"]["AllowedDomains"] = LabelUtils::LabelsToString(allowed_domains);
                json_queue.enqueue(tmp);
            }
        }
    }
}

void interpretation::Graph::Properties::NumberOfHops(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue,
    int num_hops)
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
        tmp["Query"] = graph[p[0]].query.ToString();
        tmp["Violation"]["ActualHops"] = nameServers.size();
        tmp["Violation"]["MaxAllowedHops"] = num_hops;
        json_queue.enqueue(tmp);
    }
}

void interpretation::Graph::Properties::NumberOfRewrites(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue,
    int num_rewrites)
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
            } else if (
                std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::NSNOTFOUND ||
                std::get<0>(graph[vd].answer.get()[0]) == ReturnTag::REFUSED) {
                incomplete = true;
            }
        }
    }

    if (rewrites > num_rewrites) {
        json tmp;
        tmp["Property"] = "Rewrites";
        tmp["Query"] = graph[p[0]].query.ToString();
        tmp["Violation"]["ActualRewrites"] = rewrites;
        tmp["Violation"]["MaxAllowedRewrites"] = num_rewrites;
        json_queue.enqueue(tmp);
    }
}

void interpretation::Graph::Properties::QueryRewrite(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue,
    vector<vector<NodeLabel>> domain)
{
    /*
      If there is a node with answer tag as AnsQ then the new query should be under the subdomain of domain.
    */
    for (int i = 0; i < p.size(); i++) {
        if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
            // Rewrite happened at this node
            // The answer would have a DNAME/CNAME Resource record and the new query would be availble in the next node
            // in the path.
            if (i < p.size() - 1) {
                if (!LabelUtils::SubDomainCheck(domain, graph[p[static_cast<long long>(i) + 1]].query.name)) {
                    json tmp;
                    tmp["Property"] = "Query Rewrite";
                    tmp["Query"] = graph[p[i]].query.ToString();
                    tmp["Violation"]["RewriteTarget"] = graph[p[static_cast<long long>(i) + 1]].query.ToString();
                    tmp["Violation"]["Nameserver"] = graph[p[i]].ns;
                    tmp["Violation"]["ExpectedUnder"] = LabelUtils::LabelsToString(domain);
                    json_queue.enqueue(tmp);
                }
            } else {
                // Rewrite can be the last node in the path if the user requested CNAME type.
                // Logger->error(fmt::format("properties.cpp (QueryRewrite) - An implementation error - REWRITE is the
                // last node in the path: {}", graph[p[0]].query.ToString()));
            }
        }
    }
}

void interpretation::Graph::Properties::RewriteBlackholing(
    const interpretation::Graph &graph,
    const Path &p,
    moodycamel::ConcurrentQueue<json> &json_queue)
{
    /*
      If there is a node with answer tag as REWRITE then the last node in the path should not be an NXDOMAIN.
      (REFUSED/NSNOTFOUND) If the last node is NXDOMAIN then it means the initial query is rewritten and the new query
      doesn't have resource record of any type. We may flag things which end up with REFUSED/NSNOTFOUND to be
      conservative.
    */
    for (int i = 0; i < p.size(); i++) {
        if (graph[p[i]].answer && std::get<0>(graph[p[i]].answer.get()[0]) == ReturnTag::REWRITE) {
            // Rewrite happened at this node
            if (i < p.size() - 1) {
                auto &end_tag = std::get<0>(graph[p[p.size() - 1]].answer.get()[0]);
                if (end_tag == ReturnTag::NX) {
                    auto targetNode = graph[p[p.size() - 1]];
                    json tmp;
                    tmp["Property"] = "Rewrite Blackholing";
                    tmp["Query"] = graph[p[i]].query.ToString();
                    tmp["Violation"]["RewriteTarget"] = targetNode.query.ToString();
                    tmp["Violation"]["BlackholeNameserver"] = targetNode.ns;
                    json_queue.enqueue(tmp);
                    /*	stringstream ss;
                        ss << std::this_thread::get_id();
                        if (!boost::filesystem::exists("Graphs/" + ss.str() + ".dot")) {
                            graph.GenerateDotFile("Graphs/" + ss.str() + ".dot");
                        }*/
                }
                /*else if (end_tag == ReturnTag::REFUSED || end_tag == ReturnTag::NSNOTFOUND) {
                    json tmp;
                    tmp["Property"] = "Rewrite Blackholing";
                    tmp["Equivalence Class"] = graph[p[i]].query.ToString();
                    tmp["Violation"] = "Query is eventually rewritten to " + graph[p[p.size() - 1]].query.ToString() + "
                for which there is no sufficient information at NS: " + graph[p[p.size() - 1]].ns + " to decide whether
                it is blackholed"; json_queue.enqueue(tmp);
                }*/
            } else {
                // Rewrite can be the last node in the path if the user requested CNAME type.
                // Logger->error(fmt::format("properties.cpp (RewriteBlackholing) - An implementation error - REWRITE is
                // the last node in the path: {}", graph[p[0]].query.ToString()));
            }
            break;
        }
    }
}
