#include "interpretation-properties.h"

inline string ReturntagToString(const ReturnTag &r)
{
    if (r == ReturnTag::ANS)
        return "ANS";
    else if (r == ReturnTag::REWRITE)
        return "REWRITE";
    else if (r == ReturnTag::REF)
        return "REF";
    else if (r == ReturnTag::NX)
        return "NX";
    else if (r == ReturnTag::REFUSED)
        return "REFUSED";
    else if (r == ReturnTag::NSNOTFOUND)
        return "NS Not Found";
    return "";
}

template <class NSMap, class QueryMap, class AnswerMap>
template <class Vertex>
inline void interpretation::Graph::VertexWriter<NSMap, QueryMap, AnswerMap>::operator()(ostream &out, const Vertex &v)
    const
{
    auto nameServer = get(nsm, v);
    auto query = get(qm, v);
    auto answer = get(am, v);
    // string label = "[label=\"NS: " + nameServer + " \\n Q: " + query.name + " T:"+ RRTypesToString(query.rrTypes) + "
    // \\n A:";
    string queryName = "\\n Q:";
    if (query.excluded) {
        queryName += "~{}.";
    }
    string label = "[label=\"NS: " + nameServer + queryName + LabelUtils::LabelsToString(query.name) +
                   " T:" + TypeUtils::TypesToString(query.rrTypes) + "  \\n A:";
    std::bitset<RRType::N> answerTypes;
    if (answer) {
        for (auto r : answer.get()) {
            label += ReturntagToString(std::get<0>(r));
        }
    }
    // label += RRTypesToString(answerTypes) + "\"]";
    label += "\"]";
    if (nameServer == "") {
        label = label + " [shape=diamond]";
    }
    out << label;
}

template <class EdgeMap>
template <class Edge>
inline void interpretation::Graph::EdgeWriter<EdgeMap>::operator()(ostream &out, const Edge &e) const
{
    auto type = get(wm, e);
    if (!type) {
        out << "[color=black]";
    } else {
        out << "[color=red]";
    }
}

interpretation::Graph::Graph(const EC query, const Context &context)
{
    // Add a dummy vertex as the start node over the top Name Servers
    root_ = boost::add_vertex(*this);
    (*this)[root_].ns = "";
    (*this)[root_].query = query;
    nameserver_to_vertices_map_.insert({"", std::vector<VertexDescriptor>()});
    auto it = nameserver_to_vertices_map_.find("");
    if (it != nameserver_to_vertices_map_.end()) {
        it->second.push_back(root_);
    } else {
        Logger->critical(
            fmt::format("interpretation-graph.cpp (Graph) - Unable to insert node into a interpretation graph"));
        std::exit(EXIT_FAILURE);
    }
    // Logger->debug(fmt::format("interpretation-graph (Graph) Added the dummy node {}", query.ToString()));
    StartFromTopNameservers(root_, query, context);
    // cout << "Interpretation: " << num_vertices(*this) << endl;
}

void interpretation::Graph::CheckCnameDnameAtSameNameserver(
    VertexDescriptor &current_node,
    const EC newQuery,
    const Context &context)
{
    // Logger->debug(fmt::format("interpretation-graph (CheckCnameDnameAtSameNameserver) Query:{} New query:{}",
    // (*this)[current_node].query.ToString(), newQuery.ToString())); Search for relevant zone at the same name server
    // first
    auto [valid, violation_label] = LabelUtils::LengthCheck(newQuery.name, 0);
    if (valid) {
        boost::optional<int> start = GetRelevantZone((*this)[current_node].ns, newQuery, context);
        if (start) {
            // Logger->debug(fmt::format("interpretation-graph (CheckCnameDnameAtSameNameserver) Found relevant zone
            // with id
            // {}", start.get()));
            boost::optional<VertexDescriptor> node = InsertNode((*this)[current_node].ns, newQuery, current_node, {});
            // If there was no same query earlier to this NS then continue the querying process.
            if (node) {
                // Logger->debug(fmt::format("interpretation-graph (CheckCnameDnameAtSameNameserver) Inserted new node
                // with vd {}", node.get()));
                QueryResolver(context.zoneId_to_zone.find(start.get())->second, node.get(), context);
            }
        } else {
            // Start from the top Zone file
            // Logger->debug(fmt::format("interpretation-graph (CheckCnameDnameAtSameNameserver) Did not find a relevant
            // zone"));
            StartFromTopNameservers(current_node, newQuery, context);
        }
    } else {
        boost::optional<VertexDescriptor> node = InsertNode((*this)[current_node].ns, newQuery, current_node, {});
        if (node) {
            vector<zone::LookUpAnswer> answers = {};
            answers.push_back(std::make_tuple(ReturnTag::YX, newQuery.rrTypes, vector<ResourceRecord>{}));
            (*this)[node.get()].answer = std::move(answers);
        }
    }  
}

boost::optional<interpretation::Graph::VertexDescriptor> interpretation::Graph::InsertNode(
    string ns,
    EC query,
    VertexDescriptor edge_start,
    boost::optional<VertexDescriptor> edge_query)
{
    // First checks if a node exists in the graph with NS = ns and Query = query
    // If it exists, then it add an edge from the edgeStart to found node and returns {}
    // Else Creates a new node and adds an edge and returns the new node
    auto it = nameserver_to_vertices_map_.find(ns);
    if (it == nameserver_to_vertices_map_.end()) {
        nameserver_to_vertices_map_.insert({ns, std::vector<VertexDescriptor>()});
    } else {
        std::vector<VertexDescriptor> &existingNodes = it->second;
        for (auto &n : existingNodes) {
            if (query == (*this)[n].query) {
                EdgeDescriptor e;
                bool b;
                /*if (edgeStart != n) {*/
                boost::tie(e, b) = boost::add_edge(edge_start, n, *this);
                if (!b) {
                    Logger->critical(fmt::format(
                        "interpretation-graph.cpp (InsertNode) - Unable to add edge to a interpretation graph"));
                    std::exit(EXIT_FAILURE);
                }
                if (edge_query) {
                    (*this)[e].intermediate_query = boost::make_optional(static_cast<int>(edge_query.get()));
                }
                //}
                // Logger->debug(fmt::format("interpretation-graph (InsertNode) Found duplicate node in interpretation
                // graph"));
                return {};
            }
        }
    }
    it = nameserver_to_vertices_map_.find(ns);
    if (it != nameserver_to_vertices_map_.end()) {
        VertexDescriptor v = boost::add_vertex(*this);
        (*this)[v].ns = ns;
        (*this)[v].query = query;
        // Logger->debug(fmt::format("interpretation-graph (InsertNode) Added new node with id {} to interpretation
        // graph - ns:{}, query:{}", v, (*this)[v].ns, (*this)[v].query.ToString()));
        EdgeDescriptor e;
        bool b;
        boost::tie(e, b) = boost::add_edge(edge_start, v, *this);
        if (!b) {
            Logger->critical(
                fmt::format("interpretation-graph.cpp (InsertNode) - Unable to add edge to a interpretation graph"));
            std::exit(EXIT_FAILURE);
        }
        if (edge_query) {
            (*this)[e].intermediate_query = boost::make_optional(static_cast<int>(edge_query.get()));
        }
        it->second.push_back(v);
        return v;
    } else {
        Logger->critical(
            fmt::format("interpretation-graph.cpp (InsertNode) -Unable to insert into a nameserver_to_vertices_map_"));
        std::exit(EXIT_FAILURE);
    }
}

boost::optional<int> interpretation::Graph::GetRelevantZone(string ns, const EC query, const Context &context) const
{
    auto it = context.nameserver_zoneIds_map.find(ns);
    auto queryLabels = query.name;
    // Logger->debug(fmt::format("interpretation-graph (GetRelevantZone) TopNS {} queryLabels {}", ns,
    // LabelUtils::LabelsToString(queryLabels)));
    if (it != context.nameserver_zoneIds_map.end()) {
        bool found = false;
        int max = -1;
        int bestMatch = 0;
        for (auto zid : it->second) {
            int i = 0;
            bool valid = true;
            // Logger->debug(fmt::format("interpretation-graph (GetRelevantZone) Searching zone with id {} and origin
            // {}", zid, LabelUtils::LabelsToString(context.zoneId_to_zone.find(zid)->second.get_origin())));
            for (const NodeLabel &l : context.zoneId_to_zone.find(zid)->second.get_origin()) {
                if (i >= queryLabels.size() || l.n != queryLabels[i].n) {
                    valid = false;
                    break;
                }
                i++;
            }
            if (valid) {
                // Logger->debug(fmt::format("interpretation-graph (GetRelevantZone) Found valid one with id {}", zid));
                found = true;
                if (i > max) {
                    // Logger->debug(fmt::format("interpretation-graph (GetRelevantZone) Best match updated from  {} to
                    // {}", bestMatch, zid));
                    max = i;
                    bestMatch = zid;
                }
            }
        }
        if (found)
            return bestMatch;
        else
            return {};
    } else {
        return {};
    }
}

vector<tuple<ResourceRecord, vector<ResourceRecord>>> interpretation::Graph::MatchNsGlueRecords(
    vector<ResourceRecord> records) const
{
    vector<tuple<ResourceRecord, vector<ResourceRecord>>> pairs;
    for (int i = 0; i < records.size(); i++) {
        if (records[i].get_type() == RRType::NS) {
            vector<ResourceRecord> matched;
            for (int j = i + 1; j < records.size(); j++) {
                if (records[j].get_type() == RRType::A || records[j].get_type() == RRType::AAAA) {
                    matched.push_back(records[j]);
                } else {
                    pairs.push_back(std::make_tuple(std::move(records[i]), std::move(matched)));
                    i = j - 1;
                    break;
                }
            }
            if (matched.size()) {
                pairs.push_back(std::make_tuple(std::move(records[i]), std::move(matched)));
            }
        }
    }
    return pairs;
}

void interpretation::Graph::NsSubRoutine(
    const VertexDescriptor &current_vertex,
    const string &new_ns,
    boost::optional<VertexDescriptor> edge_query,
    const Context &context)
{
    boost::optional<VertexDescriptor> node =
        InsertNode(new_ns, (*this)[current_vertex].query, current_vertex, edge_query);
    if (node) {
        auto it = context.nameserver_zoneIds_map.find(new_ns);
        if (it != context.nameserver_zoneIds_map.end()) {
            boost::optional<int> start = GetRelevantZone(new_ns, (*this)[current_vertex].query, context);
            if (start) {
                QueryResolver(context.zoneId_to_zone.find(start.get())->second, node.get(), context);
            } else {
                // Path terminates - No relevant zone file available from the NS.
                vector<zone::LookUpAnswer> answer;
                answer.push_back(
                    std::make_tuple(ReturnTag::REFUSED, (*this)[node.get()].query.rrTypes, vector<ResourceRecord>{}));
                (*this)[node.get()].answer = answer;
            }
        } else {
            // Path terminates -  The newNS not found.
            vector<zone::LookUpAnswer> answer;
            answer.push_back(
                std::make_tuple(ReturnTag::NSNOTFOUND, (*this)[node.get()].query.rrTypes, vector<ResourceRecord>{}));
            (*this)[node.get()].answer = answer;
        }
    }
}

void interpretation::Graph::PrettyPrintLoop(
    const VertexDescriptor &start,
    Path p,
    moodycamel::ConcurrentQueue<json> &json_queue) const
{
    Path loop;
    bool found = false;
    for (auto &v : p) {
        if (v == start) {
            found = true;
        }
        if (found)
            loop.push_back(v);
    }

    json tmp;
    tmp["Property"] = "Cyclic Zone Dependency";
    tmp["Loop"] = {};
    for (auto &v : loop) {
        json node;
        node["NS"] = (*this)[v].ns;
        node["Query"] = (*this)[v].query.ToString();
        if ((*this)[v].answer) {
            node["AnswerTag"] = std::get<0>((*this)[v].answer.get()[0]);
        }
        tmp["Loop"].push_back(node);
    }
    json_queue.enqueue(tmp);
}

EC interpretation::Graph::ProcessCname(const ResourceRecord &record, const EC query) const
{
    EC new_query;
    new_query.rrTypes = query.rrTypes;
    new_query.rrTypes.reset(RRType::CNAME);
    new_query.name = LabelUtils::StringToLabels(record.get_rdata());
    return new_query;
}

EC interpretation::Graph::ProcessDname(const ResourceRecord &record, const EC query) const
{
    int i = 0;
    for (auto &l : record.get_name()) {
        if (!(l == query.name[i])) {
            Logger->critical(fmt::format(
                "interpretation-graph.cpp (ProcessDname) - Query name is not a sub-domain of the DNAME record"));
            exit(EXIT_FAILURE);
        }
        i++;
    }
    vector<NodeLabel> rdata_labels = LabelUtils::StringToLabels(record.get_rdata());
    vector<NodeLabel> name_labels = query.name;
    for (; i < name_labels.size(); i++) {
        rdata_labels.push_back(name_labels[i]);
    }
    EC new_query;
    new_query.excluded = query.excluded;
    new_query.rrTypes = query.rrTypes;
    new_query.name = std::move(rdata_labels);
    return new_query;
}

void interpretation::Graph::QueryResolver(
    const zone::Graph &z,
    VertexDescriptor &current_vertex,
    const Context &context)
{
    // Logger->debug(fmt::format("interpretation-graph (QueryResolver) Query:{} look up at zone with origin-{}",
    // (*this)[current_vertex].query.ToString(), LabelUtils::LabelsToString(z.get_origin())));
    bool complete_match = false;
    (*this)[current_vertex].answer = z.QueryLookUpAtZone((*this)[current_vertex].query, complete_match);
    if ((*this)[current_vertex].answer) {
        vector<zone::LookUpAnswer> zone_lookup_answers = (*this)[current_vertex].answer.get();
        if (zone_lookup_answers.size() > 0) {
            for (auto &answer : zone_lookup_answers) {
                ReturnTag &ret = std::get<0>(answer);
                if (ret == ReturnTag::ANS) {
                    // Found Answer no further queries.
                } else if (ret == ReturnTag::NX) {
                    // Non-existent domain or Type not found Case
                } else if (ret == ReturnTag::REWRITE) {
                    ResourceRecord &record = std::get<2>(answer)[0];
                    // It will always be of size 1
                    // CNAME Case
                    if (record.get_type() == RRType::CNAME) {
                        // Logger->debug(fmt::format("interpretation-graph (QueryResolver) Query:{} look up at zone with
                        // origin-{} returned a CNAME record", (*this)[current_vertex].query.ToString(),
                        // LabelUtils::LabelsToString(z.get_origin())));
                        EC new_query = ProcessCname(record, (*this)[current_vertex].query);
                        CheckCnameDnameAtSameNameserver(current_vertex, new_query, context);
                    } else {
                        // DNAME Case
                        // Logger->debug(fmt::format("interpretation-graph (QueryResolver) Query:{} look up at zone with
                        // origin-{} returned a DNAME record", (*this)[current_vertex].query.ToString(),
                        // LabelUtils::LabelsToString(z.get_origin())));
                        EC new_query = ProcessDname(record, (*this)[current_vertex].query);
                        CheckCnameDnameAtSameNameserver(current_vertex, new_query, context);
                    }
                } else if (ret == ReturnTag::REF) {
                    // Referral to other NS case.
                    vector<tuple<ResourceRecord, vector<ResourceRecord>>> pairs =
                        MatchNsGlueRecords(std::get<2>(answer));
                    for (auto &pair : pairs) {
                        string new_ns = std::get<0>(pair).get_rdata();
                        vector<ResourceRecord> glueRecords = std::get<1>(pair);
                        // Either the Glue records have to exist or the referral to a topNameServer
                        if (glueRecords.size() ||
                            (std::find(context.top_nameservers.begin(), context.top_nameservers.end(), new_ns) !=
                             context.top_nameservers.end())) {
                            NsSubRoutine(current_vertex, new_ns, {}, context);
                        } else {
                            // Have to query for the IP address of NS
                            EC ns_query;
                            ns_query.name = LabelUtils::StringToLabels(new_ns);
                            ns_query.rrTypes[RRType::A] = 1;
                            ns_query.rrTypes[RRType::AAAA] = 1;
                            VertexDescriptor nsStart = SideQuery(ns_query, context);
                            // TODO: Check starting from this nsStart node if we got the ip records
                            NsSubRoutine(current_vertex, new_ns, nsStart, context);
                        }
                    }
                }
            }
        } else {
            // No records found - NXDOMAIN. The current path terminates.
            // Logger->debug(fmt::format("interpretation-graph (QueryResolver) Query:{} look up at zone with origin-{}
            // returned empty answer (NX)", (*this)[current_vertex].query.ToString(),
            // LabelUtils::LabelsToString(z.get_origin())));
        }
    } else {
        // This path terminates - On the assumption that you would have come to this NS by some referrral and if the
        // referral is wrong then the path should terminate.
        // Logger->debug(fmt::format("interpretation-graph (QueryResolver) Query:{} look up at zone with origin-{}
        // returned null answer", (*this)[current_vertex].query.ToString(),
        // LabelUtils::LabelsToString(z.get_origin())));
    }
}

interpretation::Graph::VertexDescriptor interpretation::Graph::SideQuery(const EC query, const Context &context)
{
    auto it = nameserver_to_vertices_map_.find("");
    if (it == nameserver_to_vertices_map_.end()) {
        nameserver_to_vertices_map_.insert({"", std::vector<VertexDescriptor>()});
    } else {
        std::vector<VertexDescriptor> &existingNodes = it->second;
        for (auto &n : existingNodes) {
            if (query == (*this)[n].query) {
                return n;
            }
        }
    }
    // Add a dummy vertex as the start node over the top Name Servers
    VertexDescriptor dummy = boost::add_vertex(*this);
    (*this)[dummy].ns = "";
    (*this)[dummy].query = query;
    it = nameserver_to_vertices_map_.find("");
    if (it != nameserver_to_vertices_map_.end()) {
        it->second.push_back(dummy);
    } else {
        Logger->critical(
            fmt::format("interpretation-graph.cpp (SideQuery) - Unable to insert into a nameserver_to_vertices_map_"));
        std::exit(EXIT_FAILURE);
    }
    StartFromTopNameservers(dummy, query, context);
    return dummy;
}

void interpretation::Graph::StartFromTopNameservers(
    VertexDescriptor edge_start_node,
    const EC query,
    const Context &context)
{
    for (string ns : context.top_nameservers) {
        // ns exists in the database.
        // Logger->debug(fmt::format("interpretation-graph (StartFromTopNameservers) Starting with TopNS {} for {}", ns,
        // query.ToString()));
        boost::optional<int> start = GetRelevantZone(ns, query, context);
        boost::optional<VertexDescriptor> node = InsertNode(ns, query, edge_start_node, {});
        if (start && node) {
            // Logger->debug(fmt::format("interpretation-graph (StartFromTopNameservers) Before calling
            // QueryResolver"));
            QueryResolver(context.zoneId_to_zone.find(start.get())->second, node.get(), context);
        } else if (node && !start) {
            vector<zone::LookUpAnswer> answer;
            answer.push_back(
                std::make_tuple(ReturnTag::REFUSED, (*this)[node.get()].query.rrTypes, vector<ResourceRecord>{}));
            (*this)[node.get()].answer = answer;
            // Logger->debug(fmt::format("interpretation-graph (StartFromTopNameservers) No relevant zone found and
            // answer is updated to REFUSED"));
        }
    }
}

bool interpretation::Graph::CheckForLoops(
    VertexDescriptor current_vertex,
    Path p,
    moodycamel::ConcurrentQueue<json> &json_queue) const
{
    /*
        Checks for loops in the graph taking into accoun the side queries.
        The function returns as soon as a single loop is found without enumerating all loops as it can lead to
       explosion. EnumeratePathsAndReturnEndNodes - currently doesn't handle sidequeries and the explosion problem is
       minimzed.
    */
    const EC &query = (*this)[current_vertex].query;
    for (auto &v : p) {
        if (v == current_vertex) {
            PrettyPrintLoop(current_vertex, p, json_queue);
            return true;
        }
    }
    p.push_back(current_vertex);
    for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(current_vertex, *this))) {
        if ((*this)[edge].intermediate_query) {
            if (CheckForLoops((*this)[edge].intermediate_query.get(), p, json_queue))
                return true;
        }
        if (CheckForLoops(edge.m_target, p, json_queue))
            return true;
    }
    return false;
}

void interpretation::Graph::CheckForLoops(moodycamel::ConcurrentQueue<json> &json_queue) const
{
    CheckForLoops(root_, Path{}, json_queue);
}

void interpretation::Graph::CheckPropertiesOnEC(
    const vector<interpretation::Graph::PathFunction> &path_functions,
    const vector<interpretation::Graph::NodeFunction> &end_node_functions,
    moodycamel::ConcurrentQueue<json> &json_queue) const
{
    vector<VertexDescriptor> end_nodes;
    EnumeratePathsAndReturnEndNodes(root_, end_nodes, Path{}, path_functions, json_queue);
    for (auto &f : end_node_functions) {
        f(*this, end_nodes, json_queue);
    }
}

void interpretation::Graph::EnumeratePathsAndReturnEndNodes(
    VertexDescriptor current_vertex,
    vector<VertexDescriptor> &end_nodes,
    Path p,
    const vector<interpretation::Graph::PathFunction> &path_functions,
    moodycamel::ConcurrentQueue<json> &json_queue) const
{
    const EC &query = (*this)[current_vertex].query;
    if ((*this)[current_vertex].ns != "") {
        // If the returnTag is a AnsQ (along with record being CNAME) and request type contains CNAME then the path ends
        // here for CNAME and this node is a leaf node with respect to t = CNAME.
        if ((*this)[current_vertex].answer && (*this)[current_vertex].answer.get().size() > 0) {
            if (std::get<0>((*this)[current_vertex].answer.get()[0]) == ReturnTag::REWRITE &&
                std::get<2>((*this)[current_vertex].answer.get()[0])[0].get_type() == RRType::CNAME &&
                query.rrTypes[RRType::CNAME]) {
                if (end_nodes.end() == std::find(end_nodes.begin(), end_nodes.end(), current_vertex)) {
                    end_nodes.push_back(current_vertex);
                }
                Path cname_path = p;
                cname_path.push_back(current_vertex);
                for (auto &f : path_functions) {
                    f(*this, cname_path, json_queue);
                }
            }
        } else {
            Logger->warn(fmt::format(
                "interpretation-graph.cpp (EnumeratePathsAndReturnEndNodes) - Empty Answer found "
                "at a node in the interpretation graph for {}",
                query.ToString()));
        }
    }
    for (auto &v : p) {
        if (v == current_vertex) {
            // cout << "loop detected for: " << LabelsToString(query.name) << endl;
            return;
        }
    }
    p.push_back(current_vertex);
    for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(current_vertex, *this))) {
        // TODO : Edges that have side query
        EnumeratePathsAndReturnEndNodes(edge.m_target, end_nodes, p, path_functions, json_queue);
    }
    if (out_degree(current_vertex, *this) == 0) {
        // Last node in the graph
        if (end_nodes.end() == std::find(end_nodes.begin(), end_nodes.end(), current_vertex)) {
            end_nodes.push_back(current_vertex);
        }
        // Path detected
        for (auto &f : path_functions) {
            f(*this, p, json_queue);
        }
    }
}

void interpretation::Graph::GenerateDotFile(const string output_file) const
{
    std::ofstream dotfile(output_file);
    write_graphviz(
        dotfile, *this,
        MakeVertexWriter(
            boost::get(&interpretation::Vertex::ns, *this), boost::get(&interpretation::Vertex::query, *this),
            boost::get(&interpretation::Vertex::answer, *this)),
        MakeEdgeWriter(boost::get(&interpretation::Edge::intermediate_query, *this)));
}
