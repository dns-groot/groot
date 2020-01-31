#pragma once
#include <string>
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <iostream>
#include <map> 
#include "resource_record.h"
#include "graph.h"
#include "zone.h"
#include "interpreter.h"
#include <nlohmann\json.hpp>


using namespace std;
using json = nlohmann::json;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef std::function<void(const InterpreterGraph&, const vector<IntpVD>&)> NodeFunction;
typedef std::function<void(const InterpreterGraph&, const Path&)> PathFunction;
typedef std::pair <VertexDescriptor, int> closestNode;
typedef tuple<vector<ResourceRecord>, vector<ResourceRecord>, vector<ResourceRecord>> CommonSymDiff;
typedef tuple<int, boost::optional<vector<ResourceRecord>>, vector<ResourceRecord>> ZoneIdGlueNSRecords;

// End node functions
void CheckResponseReturned(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq, json& output);
void CheckSameResponseReturned(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq, json& output);
void CheckResponseValue(const InterpreterGraph& graph, const vector<IntpVD>& endNodes, std::bitset<RRType::N> typesReq, set<string> values, json& output);

// Path functions
void NumberOfRewrites(const InterpreterGraph& graph, const Path& p, int num_rewrites, json& output);
void NumberOfHops(const InterpreterGraph& graph, const Path& p, int num_hops, json& output);
void CheckDelegationConsistency(const InterpreterGraph& graph, const Path& p, json& output);
void CheckLameDelegation(const InterpreterGraph& graph, const Path& p, json& output);
void QueryRewrite(const InterpreterGraph& graph, const Path& p, vector<Label>  domain, json& output);
void NameServerContact(const InterpreterGraph& graph, const Path& p, vector<Label>  domain, json& output);

// Parent-Child Synatctic Record check functions
void CheckStructuralDelegationConsistency(LabelGraph& graph, VertexDescriptor root, string userInput, boost::optional<VertexDescriptor> labelNode, json& output);
void CheckAllStructuralDelegations(LabelGraph& graph, VertexDescriptor root, string userInput, VertexDescriptor currentNode, json& output);

void LoopChecker(InterpreterGraph& graph, IntpVD start, Path p, json& output);
void GenerateECAndCheckProperties(LabelGraph& g, VertexDescriptor root, string userInput, std::bitset<RRType::N> typesReq, bool subdomain, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions, json& output);
void CheckPropertiesOnEC(EC& query, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions, json& output);
vector<closestNode> SearchNode(LabelGraph& g, VertexDescriptor closestEncloser, vector<Label>& labels, int index);

string QueryFormat(const EC& query);
CommonSymDiff CompareRRs(vector<ResourceRecord> resA, vector<ResourceRecord> resB);