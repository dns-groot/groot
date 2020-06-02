#ifndef LABEL_GRAPH_H_
#define LABEL_GRAPH_H_

//#define BOOST_GRAPH_NO_BUNDLED_PROPERTIES 1

#include "job.h"
#include "my-logger.h"

namespace label
{

struct Vertex {
    NodeLabel name;
    int16_t len = -1;
    std::bitset<RRType::N> rrtypes_available;
    std::vector<tuple<int, zone::Graph::VertexDescriptor>> zoneId_vertexId;

  private:
    friend class boost::serialization::access;
    template <typename Archive> void serialize(Archive &ar, const unsigned int version)
    {
        ar &name;
        ar &len;
        ar &rrtypes_available;
        ar &zoneId_vertexId;
    }
};

enum EdgeType { normal = 1, dname = 2 };

struct Edge {
    EdgeType type;

  private:
    friend class boost::serialization::access;
    template <typename Archive> void serialize(Archive &ar, const unsigned int version)
    {
        ar &type;
    }
};

class Graph : public boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, Vertex, Edge>
{
  public:
    using VertexDescriptor = boost::graph_traits<Graph>::vertex_descriptor;
    using ClosestNode = std::pair<VertexDescriptor, int>;

  private:
    using EdgeDescriptor = boost::graph_traits<Graph>::edge_descriptor;
    using VertexIterator = boost::graph_traits<Graph>::vertex_iterator;
    using EdgeIterator = boost::graph_traits<Graph>::edge_iterator;
    using LabelToVertex = boost::unordered_map<NodeLabel, VertexDescriptor>;
    using ZoneIdGlueNSRecords = tuple<int, boost::optional<vector<ResourceRecord>>, vector<ResourceRecord>>;

    template <class VertexMap> class VertexWriter
    {
      public:
        VertexWriter(VertexMap w) : wm(w)
        {
        }
        template <class Vertex> void operator()(ostream &out, const Vertex &v) const
        {
            auto type = get(wm, v);
            out << "[label=\"" << type.get() << "\"]";
        }

      private:
        VertexMap wm;
    };

    template <class VertexMap> inline VertexWriter<VertexMap> MakeVertexWriter(VertexMap w)
    {
        return VertexWriter<VertexMap>(w);
    }

    template <class EdgeMap> class EdgeWriter
    {
      public:
        EdgeWriter(EdgeMap w) : wm(w)
        {
        }
        template <class Edge> void operator()(ostream &out, const Edge &e) const
        {
            auto type = get(wm, e);
            if (type == normal) {
                out << "[color=black]";
            } else {
                out << "[color=red]";
            }
        }

      private:
        EdgeMap wm;
    };

    template <class EdgeMap> inline EdgeWriter<EdgeMap> MakeEdgeWriter(EdgeMap w)
    {
        return EdgeWriter<EdgeMap>(w);
    }

    boost::unordered_map<VertexDescriptor, LabelToVertex> vertex_to_child_map_;
    VertexDescriptor root_ = 0;

    VertexDescriptor AddNodes(VertexDescriptor, const vector<NodeLabel> &, int &);
    void CompareParentChildDelegationRecords(
        const std::vector<ZoneIdGlueNSRecords> &,
        const std::vector<ZoneIdGlueNSRecords> &,
        string,
        const Context &,
        Job &) const;
    void ConstructChildLabelsToVertexDescriptorMap(VertexDescriptor);
    void ConstructOutputNS(
        json &,
        const CommonSymDiff &,
        boost::optional<CommonSymDiff>,
        string,
        string,
        string,
        string) const;
    VertexDescriptor GetAncestor(VertexDescriptor, const vector<NodeLabel> &, vector<VertexDescriptor> &, int &index)
        const;
    // string GetHostingNameServer(int, const Context &) const;
    void NodeEC(const vector<NodeLabel> &name, Job &) const;
    vector<ClosestNode> SearchNode(VertexDescriptor, const vector<NodeLabel> &, int);
    void SubDomainECGeneration(VertexDescriptor, vector<NodeLabel>, bool, Job &, const Context &, bool);
    void WildcardChildEC(std::vector<NodeLabel> &, const vector<NodeLabel> &, int, Job &) const;

  public:
    void AddResourceRecord(const ResourceRecord &, const int &, zone::Graph::VertexDescriptor);
    void CheckStructuralDelegationConsistency(string, label::Graph::VertexDescriptor, const Context &, Job &);
    vector<ClosestNode> ClosestEnclosers(const vector<NodeLabel> &);
    vector<ClosestNode> ClosestEnclosers(const string &);
    void GenerateDotFile(string);
    void GenerateECs(Job &, const Context &);
    Graph();
};
} // namespace label

#endif