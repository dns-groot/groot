#ifndef INTERPRETATION_PROPERTIES_H
#define INTERPRETATION_PROPERTIES_H


#include <nlohmann/json.hpp>

#include "../concurrentqueue/concurrentqueue.h"
#include "equivalence-class.h"
#include "utils.h"
#include "zone-graph.h"
#include "context.h"

using json = nlohmann::json;

namespace interpretation {

	struct Vertex {
		std::string ns;
		EC query;
		boost::optional<vector<zone::LookUpAnswer>> answer;
		void check() { cout << "Inside Vertex --> " << &query << endl; }
	private:
		friend class boost::serialization::access;
		template <typename Archive>
		void serialize(Archive& ar, const unsigned int version) {
			ar& ns;
			ar& query;
			ar& answer;
		}
	};

	struct Edge {
		boost::optional<int> intermediate_query;
	private:
		friend class boost::serialization::access;
		template <typename Archive>
		void serialize(Archive& ar, const unsigned int version) {
			ar& intermediate_query;
		}
	};

	class Graph : public boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, Vertex, Edge> {
	public:
		using VertexDescriptor = boost::graph_traits<Graph>::vertex_descriptor;
		using EdgeDescriptor = boost::graph_traits<Graph>::edge_descriptor;
		using Path = vector<VertexDescriptor>;
		using NodeFunction = std::function<void(const Graph&, const vector<VertexDescriptor>&, moodycamel::ConcurrentQueue<json>&)>;
		using PathFunction = std::function<void(const Graph&, const Path&, moodycamel::ConcurrentQueue<json>&)>;

	private:
		template <class NSMap, class QueryMap, class AnswerMap>
		class VertexWriter {
		public:
			VertexWriter(NSMap ns, QueryMap q, AnswerMap a) : nsm(ns), qm(q), am(a) {}
			template <class Vertex>
			void operator()(ostream& out, const Vertex& v) const;
		private:
			NSMap nsm;
			QueryMap qm;
			AnswerMap am;
		};

		template <class NSMap, class QueryMap, class AnswerMap>
		inline VertexWriter<NSMap, QueryMap, AnswerMap>
			MakeVertexWriter(NSMap ns, QueryMap q, AnswerMap a) const{
			return VertexWriter<NSMap, QueryMap, AnswerMap>(ns, q, a);
		}

		template <class EdgeMap>
		class EdgeWriter {
		public:
			EdgeWriter(EdgeMap w) : wm(w) {}
			template <class Edge>
			void operator()(ostream& out, const Edge& e) const;
		private:
			EdgeMap wm;
		};

		template <class EdgeMap>
		inline EdgeWriter<EdgeMap>
			MakeEdgeWriter(EdgeMap w) const {
			return EdgeWriter<EdgeMap>(w);
		}

		Graph();
		VertexDescriptor root_ = 0;
		boost::unordered_map<string, vector<VertexDescriptor>> nameserver_to_vertices_map_;

		void CheckCnameDnameAtSameNameserver(VertexDescriptor&, const EC, const Context&);
		void CheckForLoops(VertexDescriptor, Path, moodycamel::ConcurrentQueue<json>&) const;
		void EnumeratePathsAndReturnEndNodes(VertexDescriptor, vector<VertexDescriptor>&, Path, const vector<interpretation::Graph::PathFunction>&, moodycamel::ConcurrentQueue<json>&) const;
		boost::optional<VertexDescriptor> InsertNode(string, EC, VertexDescriptor, boost::optional<VertexDescriptor>);
		boost::optional<int> GetRelevantZone(string, const EC, const Context&) const;
		vector<tuple<ResourceRecord, vector<ResourceRecord>>>  MatchNsGlueRecords(vector<ResourceRecord> records) const;
		void NsSubRoutine(const VertexDescriptor&, const string&, boost::optional<VertexDescriptor>, const Context&);
		void PrettyPrintLoop(const VertexDescriptor&, Path, moodycamel::ConcurrentQueue<json>&) const;
		EC ProcessCname(const ResourceRecord&, const EC) const;
		EC ProcessDname(const ResourceRecord&, const EC) const;
		void QueryResolver(const zone::Graph&, VertexDescriptor&, const Context&);
		VertexDescriptor SideQuery(const EC, const Context&);
		void StartFromTopNameservers(VertexDescriptor, const EC, const Context&);

	public:

		class Properties {

		private:
			static tuple<vector<ResourceRecord>, vector<ResourceRecord>> GetNSGlueRecords(const vector<ResourceRecord>&);
			static void PrettyPrintResponseValue(set<string>, set<string>&, const interpretation::Graph&, const VertexDescriptor&, json&);

		public:
			//End-node properties
			static void CheckResponseReturned(const interpretation::Graph&, const vector<VertexDescriptor>&, moodycamel::ConcurrentQueue<json>&, std::bitset<RRType::N>);
			static void CheckResponseValue(const interpretation::Graph&, const vector<VertexDescriptor>&, moodycamel::ConcurrentQueue<json>&, std::bitset<RRType::N>, set<string>);
			static void CheckSameResponseReturned(const interpretation::Graph&, const vector<VertexDescriptor>&, moodycamel::ConcurrentQueue<json>&, std::bitset<RRType::N>);

			//Path properties
			static void CheckDelegationConsistency(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&);
			static void CheckLameDelegation(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&);
			static void NameServerContact(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&, vector<vector<NodeLabel>>);
			static void NumberOfHops(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&, int);
			static void NumberOfRewrites(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&, int);
			static void QueryRewrite(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&, vector<vector<NodeLabel>>);
			static void RewriteBlackholing(const interpretation::Graph&, const Path&, moodycamel::ConcurrentQueue<json>&);
		};

		void CheckForLoops(moodycamel::ConcurrentQueue<json>&) const;
		void CheckPropertiesOnEC(const vector<interpretation::Graph::PathFunction>&, const vector<interpretation::Graph::NodeFunction>&, moodycamel::ConcurrentQueue<json>&) const;
		void GenerateDotFile(const string) const;
		Graph(const EC, const Context&);
	};
}

#endif