#ifndef ZONE_GRAPH_H_
#define ZONE_GRAPH_H_

#include <nlohmann/json.hpp>

#include "equivalence-class.h"
#include "node-label.h"

using namespace std;
using json = nlohmann::json;

enum class ReturnTag { ANS, REWRITE, REF, NX, REFUSED, NSNOTFOUND };

namespace zone
{

enum class RRAddCode { SUCCESS, DUPLICATE, CNAME_MULTIPLE, DNAME_MULTIPLE, CNAME_OTHER };

struct Vertex {
    NodeLabel name;
    vector<ResourceRecord> rrs;

  private:
    friend class boost::serialization::access;
    template <typename Archive> void serialize(Archive &ar, const unsigned int version)
    {
        ar &name;
        ar &rrs;
    }
};

using LookUpAnswer = tuple<ReturnTag, std::bitset<RRType::N>, vector<ResourceRecord>>;

class Graph : public boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, Vertex>
{
  public:
    using VertexDescriptor = boost::graph_traits<Graph>::vertex_descriptor;

  private:
    using EdgeDescriptor = boost::graph_traits<Graph>::edge_descriptor;
    using LabelToVertex = boost::unordered_map<NodeLabel, VertexDescriptor>;

    int id_;
    vector<NodeLabel> origin_;
    zone::Graph::VertexDescriptor root_ = 0;
    std::unordered_map<zone::Graph::VertexDescriptor, LabelToVertex> vertex_to_child_map_;

    Graph();

    void AddGlueRecords(vector<ResourceRecord> &) const;
    VertexDescriptor AddNodes(VertexDescriptor, const vector<NodeLabel> &, const int &);
    void ConstructChildLabelsToVertexDescriptorMap(const zone::Graph::VertexDescriptor);
    zone::Graph::VertexDescriptor GetAncestor(zone::Graph::VertexDescriptor, const vector<NodeLabel> &, int &) const;
    zone::Graph::VertexDescriptor GetAncestor(
        zone::Graph::VertexDescriptor,
        const vector<NodeLabel> &,
        vector<VertexDescriptor> &,
        int &) const;
    zone::Graph::VertexDescriptor GetClosestEncloser(zone::Graph::VertexDescriptor, const vector<NodeLabel> &, int &)
        const;
    std::bitset<RRType::N> GetNodeRRTypes(const vector<ResourceRecord> &rrs) const;
    bool WildcardMatch(VertexDescriptor, std::bitset<RRType::N> &, vector<zone::LookUpAnswer> &, const EC &) const;

  public:
    Graph(int);
    const int &get_id();
    const vector<NodeLabel> &get_origin() const;

    tuple<RRAddCode, boost::optional<zone::Graph::VertexDescriptor>> AddResourceRecord(const ResourceRecord &);
    bool CheckZoneMembership(const ResourceRecord &, const string &);
    vector<ResourceRecord> LookUpGlueRecords(const vector<ResourceRecord> &) const;
    boost::optional<vector<zone::LookUpAnswer>> QueryLookUpAtZone(const EC &, bool &) const;
    bool RequireGlueRecords(const vector<ResourceRecord> &NSRecords) const;
};
} // namespace zone

#endif