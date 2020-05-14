#include "utils.h"
#include "zone-graph.h"

zone::Graph::Graph(int zoneId)
{
    root_ = boost::add_vertex(*this);
    (*this)[root_].name.set(".");
    id_ = zoneId;
}

const int &zone::Graph::get_id()
{
    return id_;
}

const vector<NodeLabel> &zone::Graph::get_origin() const
{
    return origin_;
}

void zone::Graph::AddGlueRecords(vector<ResourceRecord> &ns_records) const
{
    vector<ResourceRecord> merged_records;
    for (auto &record : ns_records) {
        vector<NodeLabel> ns_name = LabelUtils::StringToLabels(record.get_rdata());
        int index = 0;
        merged_records.push_back(std::move(record));
        VertexDescriptor closest_encloser = GetAncestor(root_, ns_name, index);
        if (ns_name.size() == index) {
            // found the node
            for (auto &noderecords : (*this)[closest_encloser].rrs) {
                if (noderecords.get_type() == RRType::A || noderecords.get_type() == RRType::AAAA) {
                    merged_records.push_back(noderecords);
                }
            }
        }
    }
    ns_records = std::move(merged_records);
}

zone::Graph::VertexDescriptor zone::Graph::AddNodes(
    zone::Graph::VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
    const int &index)
{
    for (int i = index; i < labels.size(); i++) {
        VertexDescriptor u = boost::add_vertex(*this);
        (*this)[u].name = labels[i];
        EdgeDescriptor e;
        bool b;
        boost::tie(e, b) = boost::add_edge(closest_encloser, u, *this);
        if (!b) {
            Logger->critical(fmt::format("zone-graph.cpp (AddNodes) - Unable to add edge to the graph"));
            exit(EXIT_FAILURE);
        }
        // Only the first closestEncloser might have a map. For other cases closestEncloser will take care.
        if (vertex_to_child_map_.find(closest_encloser) != vertex_to_child_map_.end()) {
            LabelToVertex &m = vertex_to_child_map_.find(closest_encloser)->second;
            m[labels[i]] = u;
        }
        closest_encloser = u;
    }
    return closest_encloser;
}

bool checkDuplicate(const vector<ResourceRecord> &rrs, const ResourceRecord &newr)
{
    for (auto &rr : rrs) {
        if (rr.get_type() == newr.get_type()) {
            if (rr.get_ttl() == newr.get_ttl() && rr.get_rdata() == newr.get_rdata()) {
                return true;
            }
        }
    }
    return false;
}

boost::optional<zone::Graph::VertexDescriptor> zone::Graph::AddResourceRecord(const ResourceRecord &record)
{
    vector<NodeLabel> labels = record.get_name();
    int index = 0;
    vector<VertexDescriptor> vertices_to_create_maps{};
    VertexDescriptor closest_encloser = GetAncestor(root_, labels, vertices_to_create_maps, index);
    for (auto &v : vertices_to_create_maps) {
        ConstructChildLabelsToVertexDescriptorMap(v);
    }
    VertexDescriptor node = AddNodes(closest_encloser, labels, index);

    // Check for duplicate data.
    if (!checkDuplicate((*this)[node].rrs, record)) {
        (*this)[node].rrs.push_back(record);
        if (record.get_type() == RRType::SOA) {
            origin_ = record.get_name();
        }
        return node;
    } else {
        return {};
    }
}

bool zone::Graph::CheckZoneMembership(const ResourceRecord &record, const string &filename)
{
    if (record.get_type() == RRType::SOA && origin_.size() == 0) {
        return true;
    } else if (origin_.size() == 0) {
        Logger->critical(fmt::format(
            "zone-graph.cpp (CheckZoneMembership) - Non SOA record is written before a SOA record in the zone file {}",
            filename));
        exit(EXIT_FAILURE);
    }
    return LabelUtils::SubDomainCheck(origin_, record.get_name());
}

vector<ResourceRecord> zone::Graph::LookUpGlueRecords(const vector<ResourceRecord> &ns_records) const
{
    vector<ResourceRecord> ip_records;
    for (auto &record : ns_records) {
        vector<NodeLabel> ns_name = LabelUtils::StringToLabels(record.get_rdata());
        int index = 0;
        VertexDescriptor closest_encloser = GetAncestor(root_, ns_name, index);
        if (ns_name.size() == index) {
            // found the node
            for (auto &noderecords : (*this)[closest_encloser].rrs) {
                if (noderecords.get_type() == RRType::A || noderecords.get_type() == RRType::AAAA) {
                    ip_records.push_back(noderecords);
                }
            }
        }
    }
    return ip_records;
}

void zone::Graph::ConstructChildLabelsToVertexDescriptorMap(const zone::Graph::VertexDescriptor node)
{
    zone::Graph::LabelToVertex m;
    for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(node, *this))) {
        m[(*this)[edge.m_target].name] = edge.m_target;
    }
    // vertex_to_child_map_[node] = std::move(m);
    vertex_to_child_map_.insert({node, std::move(m)});
}

zone::Graph::VertexDescriptor zone::Graph::GetAncestor(
    zone::Graph::VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
    int &index) const
{
    vector<VertexDescriptor> vertices_to_create_maps{};
    return GetAncestor(closest_encloser, labels, vertices_to_create_maps, index);
}

zone::Graph::VertexDescriptor zone::Graph::GetAncestor(
    zone::Graph::VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
    vector<VertexDescriptor> &vertices_to_create_maps,
    int &index) const
{
    /*Given a domain this function returns its closest ancestor in the existing Zone Graph. This function is used for
     * building the Zone Graph. */
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
        for (VertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closest_encloser, *this))) {
            if ((*this)[v].name == labels[index]) {
                closest_encloser = v;
                index++;
                return GetAncestor(closest_encloser, labels, vertices_to_create_maps, index);
            }
        }
    }
    return closest_encloser;
}

zone::Graph::VertexDescriptor zone::Graph::GetClosestEncloser(
    zone::Graph::VertexDescriptor closest_encloser,
    const vector<NodeLabel> &labels,
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
            std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes((*this)[closest_encloser].rrs);
            // If at any node, we encoutner NS records and they are not part of authoritative data then the search stops
            // here
            // as they mark cuts along the bottom of a zone.
            if (nodeRRtypes[RRType::NS] == 1 and nodeRRtypes[RRType::SOA] != 1) {
                return closest_encloser;
            }
            return GetClosestEncloser(closest_encloser, labels, index);
        }
    } else {
        for (VertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closest_encloser, *this))) {
            if ((*this)[v].name == labels[index]) {
                std::bitset<RRType::N> nodeRRtypes = GetNodeRRTypes((*this)[v].rrs);
                index++;
                if (nodeRRtypes[RRType::NS] == 1 and nodeRRtypes[RRType::SOA] != 1) {
                    return v;
                }
                closest_encloser = v;
                return GetClosestEncloser(closest_encloser, labels, index);
            }
        }
    }
    return closest_encloser;
}

std::bitset<RRType::N> zone::Graph::GetNodeRRTypes(const vector<ResourceRecord> &rrs) const
{
    std::bitset<RRType::N> rrTypes;
    for (auto &record : rrs) {
        rrTypes[record.get_type()] = 1;
    }
    return rrTypes;
}

boost::optional<vector<zone::LookUpAnswer>> zone::Graph::QueryLookUpAtZone(const EC &query, bool &complete_match) const
{
    // Query lookup at the zone is peformed only if it is relevant

    /*int index = 0;
    for (Label l : z.origin) {
        if (index >= query.name.size() || l.n != query.name[index].n) {
            return {};
        }
        index++;
    }*/
    // Logger->debug(fmt::format("zone-graph (QueryLookUpAtZone) Query: {} look up at zone with origin: {}",
    // query.ToString(), LabelUtils::LabelsToString(origin_)));
    int index = 0;
    VertexDescriptor closest_encloser = GetClosestEncloser(root_, query.name, index);
    vector<zone::LookUpAnswer> answers;
    NodeLabel wildcard{"*"};
    if (query.name.size() != index) {
        // Logger->debug(fmt::format("zone-graph (QueryLookUpAtZone) Incomplete match case"));

        std::bitset<RRType::N> node_rr_types = GetNodeRRTypes((*this)[closest_encloser].rrs);
        // WildCard Child case - dq ∈∗ dr
        for (VertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closest_encloser, *this))) {
            if ((*this)[v].name == wildcard) {
                complete_match = true;
                vector<ResourceRecord> matchingRRs;
                std::bitset<RRType::N> queryTypesFound;
                for (auto &record : (*this)[v].rrs) {
                    if (record.get_type() == RRType::CNAME) {
                        answers.clear();
                        matchingRRs.clear();
                        matchingRRs.push_back(record);
                        // Return type is Ans if only CNAME is requested
                        if (query.rrTypes[RRType::CNAME] && query.rrTypes.count() == 1) {
                            answers.push_back(std::make_tuple(ReturnTag::ANS, node_rr_types, matchingRRs));
                        } else {
                            answers.push_back(std::make_tuple(ReturnTag::REWRITE, node_rr_types, matchingRRs));
                        }
                        return boost::make_optional(answers); // If CNAME then other records would be ignored.
                    }
                    if (query.rrTypes[record.get_type()] == 1) {
                        matchingRRs.push_back(record);
                        queryTypesFound.set(record.get_type());
                    }
                    // NS records at a wildcard node are forbidden.
                }
                if (queryTypesFound.count())
                    answers.push_back(std::make_tuple(ReturnTag::ANS, queryTypesFound, matchingRRs));
                if ((queryTypesFound ^ query.rrTypes).count()) {
                    answers.push_back(
                        std::make_tuple(ReturnTag::ANS, queryTypesFound ^ query.rrTypes, vector<ResourceRecord>{}));
                }
                return boost::make_optional(answers);
            }
        }
        complete_match = false;
        vector<ResourceRecord> ns_records;

        for (auto &record : (*this)[closest_encloser].rrs) {
            if (record.get_type() == RRType::DNAME) {
                // dr < dq ∧ DNAME ∈ T,  DNAME is a singleton type, there can be no other records of DNAME type at this
                // node.
                // Logger->debug(fmt::format("zone-graph (QueryLookUpAtZone) Incomplete match case returning a DNAME
                // {}", LabelUtils::LabelsToString(record.get_name())));
                vector<ResourceRecord> dname;
                dname.push_back(record);
                answers.push_back(std::make_tuple(ReturnTag::REWRITE, node_rr_types, dname));
                return boost::make_optional(answers);
            }
            if (record.get_type() == RRType::NS) {
                ns_records.push_back(record);
            }
        }
        // dr < dq ∧ NS ∈ T ∧ SOA ~∈ T
        if (ns_records.size() && !node_rr_types[RRType::SOA]) {
            AddGlueRecords(ns_records);
            answers.push_back(std::make_tuple(ReturnTag::REF, node_rr_types, ns_records));
        } else {
            // Logger->debug(fmt::format("zone-graph (QueryLookUpAtZone) Incomplete match case returning a NX"));
            answers.push_back(std::make_tuple(ReturnTag::NX, query.rrTypes, vector<ResourceRecord>{}));
        }
        return boost::make_optional(answers);
    } else {
        complete_match = true;
        if (query.excluded) {

            std::bitset<RRType::N> node_rr_types = GetNodeRRTypes((*this)[closest_encloser].rrs);
            vector<ResourceRecord> ns_records;
            for (auto &record : (*this)[closest_encloser].rrs) {
                if (record.get_type() == RRType::NS) {
                    ns_records.push_back(record);
                }
            }
            // If there are NS records, then get their glue records too.
            if (node_rr_types[RRType::NS]) {
                AddGlueRecords(ns_records);
            }
            // Referral case (Zone-Cut)
            if (node_rr_types[RRType::NS] && !node_rr_types[RRType::SOA]) {
                answers.push_back(std::make_tuple(ReturnTag::REF, node_rr_types, ns_records));
                return boost::make_optional(answers);
            }
            // If there is query excluded and the node has a wildcard then return records matching the query type
            // If there is no wildcard child then its a NXDOMAIN Case
            // If there is wildcard and non matching types then its a Ans with empty records
            for (VertexDescriptor v : boost::make_iterator_range(adjacent_vertices(closest_encloser, *this))) {
                if ((*this)[v].name == wildcard) {
                    vector<ResourceRecord> matching_rrs;
                    std::bitset<RRType::N> query_types_found;
                    for (auto &record : (*this)[v].rrs) {
                        if (record.get_type() == RRType::CNAME) {
                            answers.clear();
                            matching_rrs.clear();
                            matching_rrs.push_back(record);
                            // Return type is Ans if only CNAME is requested
                            if (query.rrTypes[RRType::CNAME] && query.rrTypes.count() == 1) {
                                answers.push_back(std::make_tuple(ReturnTag::ANS, node_rr_types, matching_rrs));
                            } else {
                                answers.push_back(std::make_tuple(ReturnTag::REWRITE, node_rr_types, matching_rrs));
                            }
                            return boost::make_optional(answers); // If CNAME then other records would be ignored.
                        }
                        if (query.rrTypes[record.get_type()] == 1) {
                            matching_rrs.push_back(record);
                            query_types_found.set(record.get_type());
                        }
                        // NS records at a wildcard node are forbidden.
                    }
                    if (query_types_found.count())
                        answers.push_back(std::make_tuple(ReturnTag::ANS, query_types_found, matching_rrs));
                    if ((query_types_found ^ query.rrTypes).count()) {
                        answers.push_back(std::make_tuple(
                            ReturnTag::ANS, query_types_found ^ query.rrTypes, vector<ResourceRecord>{}));
                    }
                    return boost::make_optional(answers);
                }
            }
            // Logger->debug(fmt::format("zone-graph (QueryLookUpAtZone) Complete match case with excluded returning a
            // NX"));
            answers.push_back(std::make_tuple(ReturnTag::NX, query.rrTypes, vector<ResourceRecord>{}));
            return boost::make_optional(answers);
        } else {
            // Exact Query Match d_r = d_q

            std::bitset<RRType::N> node_rr_types = GetNodeRRTypes((*this)[closest_encloser].rrs);
            vector<ResourceRecord> matching_rrs; // All the RRs requested by query types except NS
            vector<ResourceRecord> ns_records;
            std::bitset<RRType::N> query_types_found;
            for (auto &record : (*this)[closest_encloser].rrs) {
                if (record.get_type() == RRType::NS) {
                    ns_records.push_back(record);
                } else if (query.rrTypes[record.get_type()] == 1) {
                    query_types_found.set(record.get_type());
                    matching_rrs.push_back(record);
                }
                if (record.get_type() == RRType::CNAME) {
                    // CNAME Case
                    answers.clear();
                    matching_rrs.clear();
                    matching_rrs.push_back(record);
                    // Return type is Ans if only CNAME is requested
                    if (query.rrTypes[RRType::CNAME] && query.rrTypes.count() == 1) {
                        answers.push_back(std::make_tuple(ReturnTag::ANS, node_rr_types, matching_rrs));
                    } else {
                        answers.push_back(std::make_tuple(ReturnTag::REWRITE, node_rr_types, matching_rrs));
                    }
                    return boost::make_optional(answers); // If CNAME then other records would be ignored.
                }
            }
            // If there are NS records, then get their glue records too.
            if (node_rr_types[RRType::NS]) {
                AddGlueRecords(ns_records);
            }
            // Referral case
            if (node_rr_types[RRType::NS] && !node_rr_types[RRType::SOA]) {
                answers.push_back(std::make_tuple(ReturnTag::REF, node_rr_types, ns_records));
                return boost::make_optional(answers);
            }
            // Add the NS and glue records if the user requested them.
            if (query.rrTypes[RRType::NS] && ns_records.size()) {
                matching_rrs.insert(matching_rrs.end(), ns_records.begin(), ns_records.end());
                query_types_found.set(RRType::NS);
            }
            // Exact Type match
            if (query_types_found.count())
                answers.push_back(std::make_tuple(ReturnTag::ANS, query_types_found, matching_rrs));
            if ((query_types_found ^ query.rrTypes).count()) {
                answers.push_back(
                    std::make_tuple(ReturnTag::ANS, query_types_found ^ query.rrTypes, vector<ResourceRecord>{}));
            }
            return boost::make_optional(answers);
        }
    }
}

bool zone::Graph::RequireGlueRecords(const vector<ResourceRecord> &ns_records) const
{
    for (auto &record : ns_records) {
        vector<NodeLabel> ns_name = LabelUtils::StringToLabels(record.get_rdata());
        if (ns_name.size() < origin_.size())
            continue;
        int i = 0;
        for (; i < origin_.size(); i++) {
            if (origin_[i].get() != ns_name[i].get()) {
                break;
            }
        }
        if (i == origin_.size())
            return true;
    }
    return false;
}
