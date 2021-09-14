#include "label-graph.h"
#include "ec-task.h"
#include "structural-task.h"
#include "utils.h"

label::Graph::Graph()
{
    root_ = boost::add_vertex(*this);
    (*this)[root_].name.set(".");
}

void label::Graph::ConstructChildLabelsToVertexDescriptorMap(VertexDescriptor node)
{
    LabelToVertex m;
    for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(node, *this))) {
        if ((*this)[edge].type == normal) {
            m[(*this)[edge.m_target].name] = edge.m_target;
        }
    }
    vertex_to_child_map_.insert({node, std::move(m)});
}

label::Graph::VertexDescriptor label::Graph::GetAncestor(
    label::Graph::VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
    vector<VertexDescriptor> &vertices_to_create_maps,
    int &index) const
{
    if (labels.size() == index) {
        return closest_encloser;
    }
    if (vertex_to_child_map_.find(closest_encloser) != vertex_to_child_map_.end()) {
        const LabelToVertex &m = vertex_to_child_map_.find(closest_encloser)->second;
        auto it = m.find(labels[index]);
        if (it != m.end()) {
            closest_encloser = it->second;
            index++;
            return GetAncestor(closest_encloser, labels, vertices_to_create_maps, index);
        }
    } else {
        if (out_degree(closest_encloser, *this) > kHashMapThreshold) {
            vertices_to_create_maps.push_back(closest_encloser);
        }
        for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closest_encloser, *this))) {
            if ((*this)[edge].type == normal) {
                if ((*this)[edge.m_target].name == labels[index]) {
                    closest_encloser = edge.m_target;
                    index++;
                    return GetAncestor(closest_encloser, labels, vertices_to_create_maps, index);
                }
            }
        }
    }
    return closest_encloser;
}

/* string label::Graph::GetHostingNameServer(int zoneId, const Context &context) const
{
    // Given a zoneId return the name server which hosts that zone.
    for (auto const &[ns, zoneIds] : context.nameserver_zoneIds_map) {
        if (std::find(zoneIds.begin(), zoneIds.end(), zoneId) != zoneIds.end()) {
            return ns;
        }
    }
    Logger->critical(fmt::format(
        "label-graph.cpp (GetHostingNameServer) - ZoneId {} not found in the Name Server ZoneIds map", zoneId));
    exit(EXIT_FAILURE);
} */

void label::Graph::NodeEC(const vector<NodeLabel> &name, Job &current_job) const
{
    unique_ptr<ECTask> present = make_unique<ECTask>();
    present->ec_.name = name;
    present->ec_.rrTypes = current_job.types_req;
    // EC generated
    // Push it to the EC queue
    current_job.ec_queue.enqueue(std::move(present));
}

label::Graph::VertexDescriptor label::Graph::AddNodes(
    label::Graph::VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
    int &index)
{

    for (int i = index; i < labels.size(); i++) {
        VertexDescriptor u = boost::add_vertex(*this);
        (*this)[u].name = labels[i];
        EdgeDescriptor e;
        bool b;
        boost::tie(e, b) = boost::add_edge(closest_encloser, u, *this);
        if (!b) {
            Logger->critical(fmt::format("label-graph.cpp (AddNodes) - Unable to add edge to label graph"));
            exit(EXIT_FAILURE);
        }
        (*this)[e].type = normal;
        // Only the first closest_encloser might have a map. For other cases closest_encloser will take care.
        if (vertex_to_child_map_.find(closest_encloser) != vertex_to_child_map_.end()) {
            LabelToVertex &m = vertex_to_child_map_.find(closest_encloser)->second;
            m[labels[i]] = u;
        }
        closest_encloser = u;
    }
    return closest_encloser;
}

void label::Graph::AddResourceRecord(
    const ResourceRecord &record,
    const int &zoneId,
    zone::Graph::VertexDescriptor zone_vertexId)
{
    if (record.get_type() != RRType::N) {
        int index = 0;
        vector<NodeLabel> name_labels = record.get_name();
        vector<VertexDescriptor> vertices_to_create_maps{};
        VertexDescriptor closest_encloser = GetAncestor(root_, name_labels, vertices_to_create_maps, index);
        for (auto &v : vertices_to_create_maps) {
            ConstructChildLabelsToVertexDescriptorMap(v);
        }
        vertices_to_create_maps.clear();
        VertexDescriptor main_node = AddNodes(closest_encloser, name_labels, index);
        if (record.get_type() == RRType::DNAME) {
            vector<NodeLabel> labels = LabelUtils::StringToLabels(record.get_rdata());
            index = 0;
            closest_encloser = GetAncestor(root_, labels, vertices_to_create_maps, index);
            for (auto &v : vertices_to_create_maps) {
                ConstructChildLabelsToVertexDescriptorMap(v);
            }
            VertexDescriptor second_node = AddNodes(closest_encloser, labels, index);
            // Check if DNAME edge was added due to another zone file
            bool duplicate = false;
            for (const EdgeDescriptor& edge : boost::make_iterator_range(out_edges(main_node, *this))) {
                if (edge.m_target == second_node && (*this)[edge].type == dname) {
                    duplicate = true;
                }
            }
            if (!duplicate) {
                EdgeDescriptor e;
                bool b;
                boost::tie(e, b) = boost::add_edge(main_node, second_node, *this);
                if (!b) {
                    Logger->critical(
                        fmt::format("label-graph.cpp (AddResourceRecord) - Unable to add edge to label graph"));
                    exit(EXIT_FAILURE);
                }
                (*this)[e].type = dname;
            }
        }
        auto it = std::find_if(
            (*this)[main_node].zoneId_vertexId.begin(), (*this)[main_node].zoneId_vertexId.end(),
            [=](const std::tuple<int, zone::Graph::VertexDescriptor> &e) {
                return std::get<0>(e) == zoneId && std::get<1>(e) == zone_vertexId;
            });
        if (it == (*this)[main_node].zoneId_vertexId.end())
            (*this)[main_node].zoneId_vertexId.push_back(
                tuple<int, zone::Graph::VertexDescriptor>(zoneId, zone_vertexId));
        (*this)[main_node].rrtypes_available.set(record.get_type());
    }
}

void label::Graph::CheckStructuralDelegationConsistency(
    string user_input,
    label::Graph::VertexDescriptor node,
    const Context &context,
    Job &current_job)
{
    auto zoneId_vertexIds = (*this)[node].zoneId_vertexId;
    auto types = (*this)[node].rrtypes_available;
    if (types[RRType::NS] == 1) {
        std::vector<ZoneIdGlueNSRecords> parents;
        std::vector<ZoneIdGlueNSRecords> children;
        vector<NodeLabel> child_zone;
        vector<NodeLabel> longest_parent;
        for (auto zoneId_vertexId_pair : zoneId_vertexIds) {
            if (context.zoneId_to_zone.find(std::get<0>(zoneId_vertexId_pair)) != context.zoneId_to_zone.end()) {
                const zone::Graph &z = context.zoneId_to_zone.find(std::get<0>(zoneId_vertexId_pair))->second;
                vector<ResourceRecord> ns_records;
                bool soa = false;
                for (auto record : z[std::get<1>(zoneId_vertexId_pair)].rrs) {
                    if (record.get_type() == RRType::SOA)
                        soa = true;
                    if (record.get_type() == RRType::NS)
                        ns_records.push_back(record);
                }
                if (soa) {
                    /*
                        The RequireGlueRecords check is necessary as the child may not need glue records but the parent
                       might in which case we don't need to compare the glue record consistency. This case might arise
                       for example in ucla.edu the NS for zone cs.ucla.edu may be listed as ns1.ee.ucla.edu, then
                       ucla.edu requires a glue record but not cs.ucla.edu zone. RequireGLueRecords returns true if at
                       least one glue record is required.
                    */
                    child_zone = z.get_origin();
                    if (z.RequireGlueRecords(ns_records)) {
                        children.push_back(std::make_tuple(
                            std::get<0>(zoneId_vertexId_pair), z.LookUpGlueRecords(ns_records), std::move(ns_records)));
                    } else {
                        children.push_back(std::make_tuple(
                            std::get<0>(zoneId_vertexId_pair), boost::optional<vector<ResourceRecord>>{},
                            std::move(ns_records)));
                    }
                } else {
                    // Parents issue: For a domain - dns.foo.com.ar. the records can come from multiple sources like
                    // from com.ar. (glue record), foo.com.ar. (delegation) and dns.foo.com.ar (SOA) In such cases we
                    // get multiple parents which should not be accounted for.
                    if (longest_parent.size()) {
                        if (longest_parent == z.get_origin()) {
                            parents.push_back(std::make_tuple(
                                std::get<0>(zoneId_vertexId_pair), z.LookUpGlueRecords(ns_records),
                                std::move(ns_records)));
                        } else if (LabelUtils::SubDomainCheck(longest_parent, z.get_origin())) {
                            parents.clear();
                            longest_parent = z.get_origin();
                            parents.push_back(std::make_tuple(
                                std::get<0>(zoneId_vertexId_pair), z.LookUpGlueRecords(ns_records),
                                std::move(ns_records)));
                        }
                    } else {
                        longest_parent = z.get_origin();
                        parents.push_back(std::make_tuple(
                            std::get<0>(zoneId_vertexId_pair), z.LookUpGlueRecords(ns_records), std::move(ns_records)));
                    }
                }
            } else {
                Logger->critical(fmt::format("label-graph.cpp (CheckStructuralDelegationConsistency) - ZoneId {} not "
                                             "found in the Name Server ZoneIds map"));
                exit(EXIT_FAILURE);
            }
        }
        // Remove unnecessary glue records from parents - This case arises when some of the NS records require glue
        // records at child but not all unlike the parent.
        if (child_zone.size()) {
            for (auto &p : parents) {
                if (std::get<1>(p)) {
                    auto &glue_rrs = std::get<1>(p).get();
                    for (auto it = glue_rrs.begin(); it != glue_rrs.end();) {
                        if (!LabelUtils::SubDomainCheck(child_zone, it->get_name())) {
                            it = glue_rrs.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
        // Compare the children and parents records
        CompareParentChildDelegationRecords(parents, children, user_input, context, current_job);
    } else {
    }
}

void label::Graph::CompareParentChildDelegationRecords(
    const std::vector<ZoneIdGlueNSRecords> &parent,
    const std::vector<ZoneIdGlueNSRecords> &child,
    string user_input,
    const Context &context,
    Job &current_job) const
{
    // ZoneId, GlueRecords, NSRecords
    json j;
    for (auto &p : parent) {
        for (auto &c : child) {
            auto nsDiff = RRUtils::CompareRRs(std::get<2>(p), std::get<2>(c));
            if (std::get<1>(c)) {
                auto glueDiff = RRUtils::CompareRRs(std::get<1>(p).get(), std::get<1>(c).get());
                ConstructOutputNS(
                    j, nsDiff, glueDiff, context.zoneId_nameserver_map.at(std::get<0>(p)),
                    context.zoneId_nameserver_map.at(std::get<0>(c)), "parent", "child");
            } else {
                ConstructOutputNS(
                    j, nsDiff, {}, context.zoneId_nameserver_map.at(std::get<0>(p)),
                    context.zoneId_nameserver_map.at(std::get<0>(c)), "parent", "child");
            }
        }
    }

    for (auto it = parent.begin(); it != parent.end(); ++it) {
        for (auto itp = it + 1; itp != parent.end(); ++itp) {
            auto nsDiff = RRUtils::CompareRRs(std::get<2>(*it), std::get<2>(*itp));
            auto glueDiff = RRUtils::CompareRRs(std::get<1>(*it).get(), std::get<1>(*itp).get());
            ConstructOutputNS(
                j, nsDiff, glueDiff, context.zoneId_nameserver_map.at(std::get<0>(*it)),
                context.zoneId_nameserver_map.at(std::get<0>(*itp)), "parent-a", "parent-b");
        }
    }
    for (auto it = child.begin(); it != child.end(); ++it) {
        for (auto itp = it + 1; itp != child.end(); ++itp) {
            auto nsDiff = RRUtils::CompareRRs(std::get<2>(*it), std::get<2>(*itp));
            auto glueDiff = RRUtils::CompareRRs(std::get<1>(*it).get_value_or({}), std::get<1>(*itp).get_value_or({}));
            ConstructOutputNS(
                j, nsDiff, glueDiff, context.zoneId_nameserver_map.at(std::get<0>(*it)),
                context.zoneId_nameserver_map.at(std::get<0>(*itp)), "child-a", "child-b");
        }
    }
    // if (parent.empty() && !child.empty()) {
    //    if (j.find("Inconsistent Pairs") == j.end()) {
    //        j["Inconsistent Pairs"] = {};
    //    }
    //    json tmp;
    //    tmp["Warning"] = "There are no NS records at the parent or parent zone file is missing";
    //    tmp["Child NS"] = {};
    //    for (auto c : child)
    //        tmp["Child NS"].push_back(context.zoneId_nameserver_map.at(std::get<0>(c)));
    //    j["Inconsistent Pairs"].push_back(tmp);
    //}
    if (j.size()) {
        j["Property"] = "Structural Delegation Consistency";
        j["Domain Name"] = user_input;
        current_job.json_queue.enqueue(j);
    }
}

void label::Graph::ConstructOutputNS(
    json &j,
    const CommonSymDiff &ns_diff,
    boost::optional<CommonSymDiff> glue_diff,
    string nameserver_a,
    string nameserver_b,
    string a,
    string b) const
{
    if (std::get<1>(ns_diff).empty() && std::get<2>(ns_diff).empty() &&
        ((glue_diff && std::get<1>(glue_diff.get()).empty() && std::get<2>(glue_diff.get()).empty()) || !glue_diff)) {
        return;
    }
    if (j.find("Inconsistent Pairs") == j.end()) {
        j["Inconsistent Pairs"] = {};
    }
    json diffAB;
    diffAB[a + " NS"] = nameserver_a;
    diffAB[b + " NS"] = nameserver_b;
    if (!std::get<0>(ns_diff).empty()) {
        diffAB["Common NS Records"] = {};
        for (auto r : std::get<0>(ns_diff)) {
            diffAB["Common NS Records"].push_back(r.toString());
        }
    }
    if (!std::get<1>(ns_diff).empty()) {
        diffAB["Exclusive " + a + " NS Records"] = {};
        for (auto r : std::get<1>(ns_diff)) {
            diffAB["Exclusive " + a + " NS Records"].push_back(r.toString());
        }
    }
    if (!std::get<2>(ns_diff).empty()) {
        diffAB["Exclusive " + b + " NS Records"] = {};
        for (auto r : std::get<2>(ns_diff)) {
            diffAB["Exclusive " + b + " NS Records"].push_back(r.toString());
        }
    }
    if (glue_diff) {
        if (!std::get<0>(glue_diff.get()).empty()) {
            diffAB["Common Glue Records"] = {};
            for (auto r : std::get<0>(glue_diff.get())) {
                diffAB["Common Glue Records"].push_back(r.toString());
            }
        }
        if (!std::get<1>(glue_diff.get()).empty()) {
            diffAB["Exclusive " + a + " Glue Records"] = {};
            for (auto r : std::get<1>(glue_diff.get())) {
                diffAB["Exclusive " + a + " Glue Records"].push_back(r.toString());
            }
        }
        if (!std::get<2>(glue_diff.get()).empty()) {
            diffAB["Exclusive " + b + " Glue Records"] = {};
            for (auto r : std::get<2>(glue_diff.get())) {
                diffAB["Exclusive " + b + " Glue Records"].push_back(r.toString());
            }
        }
    }
    j["Inconsistent Pairs"].push_back(diffAB);
}

vector<label::Graph::ClosestNode> label::Graph::ClosestEnclosers(const vector<NodeLabel> &labels)
{
    return SearchNode(root_, labels, 0);
}

vector<label::Graph::ClosestNode> label::Graph::ClosestEnclosers(const string &domain_name)
{
    return ClosestEnclosers(LabelUtils::StringToLabels(domain_name));
}

vector<label::Graph::ClosestNode> label::Graph::SearchNode(
    VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
    int index)
{
    /*
        Given a user input the functions returns all the closest enclosers along with the number of labels matched.
        The function returns only the longest matching enclosers as the smaller ones would be automatically dealt by the
       ECs generated from longest ones.
    */
    vector<ClosestNode> enclosers;
    if (labels.size() == index) {
        enclosers.push_back(ClosestNode{closest_encloser, index});
        return enclosers;
    }
    if (index == (*this)[closest_encloser].len) {
        // Loop detected
        // TODO: Trace the loop to get the usr input.
        enclosers.push_back(ClosestNode{closest_encloser, -1});
        return enclosers;
    }

    int16_t before_len = (*this)[closest_encloser].len;
    (*this)[closest_encloser].len = index;
    // The current node could also be the closest encloser if no child matches.
    enclosers.push_back(ClosestNode{closest_encloser, index});
    for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closest_encloser, *this))) {
        if ((*this)[edge].type == dname) {
            auto r = SearchNode(edge.m_target, labels, index);
            enclosers.insert(enclosers.end(), r.begin(), r.end());
        }
    }
    if (vertex_to_child_map_.find(closest_encloser) != vertex_to_child_map_.end()) {
        // Check if a hash-map exists for child labels.
        LabelToVertex &m = vertex_to_child_map_.find(closest_encloser)->second;
        auto it = m.find(labels[index]);
        if (it != m.end()) {
            index++;
            auto r = SearchNode(it->second, labels, index);
            enclosers.insert(enclosers.end(), r.begin(), r.end());
        } else {
            // label[index] does not exist
        }
    } else {
        for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closest_encloser, *this))) {
            if ((*this)[edge].type == normal) {
                if ((*this)[edge.m_target].name == labels[index]) {
                    index++;
                    auto r = SearchNode(edge.m_target, labels, index);
                    enclosers.insert(enclosers.end(), r.begin(), r.end());
                    break;
                }
            }
        }
    }
    (*this)[closest_encloser].len = before_len;
    int max = -1;
    for (auto &r : enclosers) {
        if (r.second > max) {
            max = r.second;
        }
    }
    vector<ClosestNode> actual_enclosers;
    if (max != -1) {
        for (auto &r : enclosers) {
            if (r.second == max) {
                actual_enclosers.push_back(r);
            }
        }
    }
    return actual_enclosers;
}

void label::Graph::SubDomainECGeneration(
    VertexDescriptor start,
    vector<NodeLabel> parent_domain_name,
    bool skipLabel,
    Job &current_job,
    const Context &context,
    bool check_structural_delegations)
{
    // Breaking the EC generation if there are too many ECs due to mulitple DNAME loops
    if (current_job.stats.ec_count > 1000) {
        return;
    }

    size_t len = 0;
    for (NodeLabel &l : parent_domain_name) {
        len += l.get().length() + 1;
    }
    if (len >= kMaxDomainLength || (len == (*this)[start].len && skipLabel)) {
        //return if length exceedes or DNAME loop detected 
        //mostly likely turns out as cyclic zone dependecy so no need to report here
        return;
    }
    NodeLabel node_labels = (*this)[start].name;
    vector<NodeLabel> name;
    name = parent_domain_name;
    if (node_labels.get() == ".") {
        // Root has empty parent vector
    } else if (parent_domain_name.size() == 0) {
        // Empty name vector implies root
        name.push_back(node_labels);
    } else if (!skipLabel) {
        // DNAME can not occur at the root.
        name.push_back(node_labels);
    }
    int16_t beforeLen = (*this)[start].len;
    size_t nodeLen = 0;
    for (NodeLabel &l : name) {
        nodeLen += l.get().length() + 1;
    }
    if (nodeLen >= kMaxDomainLength) {
        return;
    }
    (*this)[start].len = static_cast<int16_t>(nodeLen);
    std::vector<NodeLabel> children_labels;
    std::optional<VertexDescriptor> wildcard_node;

    // Check structural delegation for this node
    if (check_structural_delegations) {
        unique_ptr<StructuralTask> task = make_unique<StructuralTask>(start, LabelUtils::LabelsToString(name));
        current_job.ec_queue.enqueue(std::move(task));
    }

    for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(start, *this))) {
        if ((*this)[edge].type == normal) {
            if ((*this)[edge.m_target].name.get() == "*") {
                wildcard_node = edge.m_target;
            }
            SubDomainECGeneration(edge.m_target, name, false, current_job, context, check_structural_delegations);
        }
        if ((*this)[edge].type == dname) {
            SubDomainECGeneration(edge.m_target, name, true, current_job, context, false);
        }
    }
    (*this)[start].len = beforeLen;
    if (!skipLabel) {
        // EC for the node if the node is not skipped.
        NodeEC(name, current_job);
    }
    // wildcardNode is useful when we want to generate only positive queries and avoid the negations.
    if (wildcard_node) {
        WildcardChildEC(children_labels, name, static_cast<int>(name.size()), current_job);
    } else {
        // Non-existent child category
        unique_ptr<ECTask> nonExistent = make_unique<ECTask>();
        nonExistent->ec_.name = name;
        nonExistent->ec_.rrTypes.flip();
        nonExistent->ec_.excluded = boost::make_optional(std::move(children_labels));
        nonExistent->ec_.nonExistent = true;
        // EC generated
        // Push it to the queue
        current_job.ec_queue.enqueue(std::move(nonExistent));
    }
}

void label::Graph::WildcardChildEC(
    std::vector<NodeLabel> &children_labels,
    const vector<NodeLabel> &labels,
    int index,
    Job &current_job) const
{
    unique_ptr<ECTask> wildcard_match = make_unique<ECTask>();
    wildcard_match->ec_.name.clear();
    for (int i = 0; i < index; i++) {
        wildcard_match->ec_.name.push_back(labels[i]);
    }
    wildcard_match->ec_.rrTypes = current_job.types_req;
    wildcard_match->ec_.excluded = boost::make_optional(std::move(children_labels));
    // EC generated - Push it to the queue
    current_job.ec_queue.enqueue(std::move(wildcard_match));
}

void label::Graph::GenerateDotFile(string output_file)
{
    std::ofstream dotfile(output_file);
    write_graphviz(
        dotfile, *this, MakeVertexWriter(boost::get(&label::Vertex::name, *this)),
        MakeEdgeWriter(boost::get(&label::Edge::type, *this)));
}

void label::Graph::GenerateECs(Job &current_job, const Context &context)
{
    // Given an user input for domain and query types, the function searches for relevant node
    // The search is relevant even for subdomain = False as we want to know the exact EC

    vector<NodeLabel> labels = LabelUtils::StringToLabels(current_job.user_input_domain);
    if (!std::get<0>(LabelUtils::LengthCheck(labels, 2))) {
        return;
    }
    vector<ClosestNode> closest_enclosers = ClosestEnclosers(labels);
    if (closest_enclosers.size()) {
        // cross-check
        int matchedIndex = closest_enclosers[0].second;
        for (auto &r : closest_enclosers) {
            if (r.second != matchedIndex) {
                Logger->critical(fmt::format(
                    "label-graph.cpp (GenerateECs) - Multiple closestEnclosers with different lengths for {}",
                    current_job.user_input_domain));
                exit(EXIT_FAILURE);
            }
        }
        if (labels.size() == matchedIndex) {
            if (current_job.check_subdomains == true) {
                vector<NodeLabel> parent_domain_name = labels;
                if (parent_domain_name.size())
                    parent_domain_name.pop_back();
                vector<std::thread> ECproducers;
                if (current_job.ec_queue.size_approx() != 0) {
                    Logger->warn(fmt::format(
                        "label-graph.cpp (GenerateECs) - The global EC queue is non-empty for {}",
                        current_job.user_input_domain));
                }
                // EC producer threads
                for (auto &encloser : closest_enclosers) {
                    ECproducers.push_back(thread([&]() {
                        SubDomainECGeneration(
                            encloser.first, parent_domain_name, false, current_job, context,
                            current_job.check_structural_delegations);
                    }));
                }
                for (auto &t : ECproducers) {
                    t.join();
                }
            } else {
                NodeEC(labels, current_job);
            }
        } else {
            // Sub-domain queries can not be peformed.
            if (current_job.check_subdomains) {
                Logger->warn(fmt::format(
                    "label-graph.cpp (GenerateECs) - The complete domain {} doesn't exist so sub-domain is not valid",
                    current_job.user_input_domain));
                if (current_job.check_structural_delegations) {
                    Logger->warn(fmt::format(
                        "label-graph.cpp (GenerateECs) - The complete domain {} doesn't exist so "
                        "structural delegation check is not valid",
                        current_job.user_input_domain));
                }
            }
            // The query might match a "wildcard" or its part of non-existent child nodes. We just set excluded to know
            // there is some negation set there.
            unique_ptr<ECTask> non_existent = make_unique<ECTask>();
            non_existent->ec_.name.clear();
            for (int i = 0; i < matchedIndex; i++) {
                non_existent->ec_.name.push_back(labels[i]);
            }
            non_existent->ec_.rrTypes = current_job.types_req;
            non_existent->ec_.excluded = boost::make_optional(std::vector<NodeLabel>());
            // EC generated
            current_job.ec_queue.enqueue(std::move(non_existent));
        }
    } else {
        Logger->critical(fmt::format(
            "label-graph.cpp (GenerateECs) - No closestEnclosers found for {}", current_job.user_input_domain));
        exit(EXIT_FAILURE);
    }
    current_job.finished_ec_generation = true;
}
