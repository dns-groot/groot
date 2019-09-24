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
#include "RR.h"
#include "graph.h"
#include "zone.h"
#include "interpreter.h"

using namespace std;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)> NodeFunction;
typedef std::function<void(const InterpreterGraph&, const Path&)> PathFunction;

// End node functions
void CheckResponseReturned(const InterpreterGraph& graph, const vector<InterpreterVertexDescriptor>& endNodes, std::bitset<RRType::N> typesReq);
void CheckSameResponseReturned(const InterpreterGraph& graph, const vector<InterpreterVertexDescriptor>& endNodes, std::bitset<RRType::N> typesReq);
void CheckResponseValue(const InterpreterGraph& graph, const vector<InterpreterVertexDescriptor>& endNodes, std::bitset<RRType::N> typesReq, vector<string> value);

// Path functions
void NumberOfRewrites(const InterpreterGraph& graph, const Path& p, int num_rewrites);
void NumberOfHops(const InterpreterGraph& graph, const Path& p, int num_hops);
void CheckDelegationConsistency(const InterpreterGraph& graph, const Path& p);
void CheckLameDelegation(const InterpreterGraph& graph, const Path& p);


void GenerateECAndCheckProperties(LabelGraph& g, VertexDescriptor root, string userInput, std::bitset<RRType::N> typesReq, bool subdomain, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions);
void CheckPropertiesOnEC(EC& query, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions);

string QueryFormat(const EC& query);