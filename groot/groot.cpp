#include <boost/algorithm/string.hpp> 
#include <cstdlib> 
#include <iostream> 
#include <string> 
#include <vector>
#include <fstream>
#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/topological_sort.hpp"
#include <boost/graph/graphviz.hpp>
#include <nlohmann/json.hpp>
#include "../groot_lib/zone.h"
#include "../groot_lib/graph.h"
#include "../groot_lib/graph.h"
#include "../groot_lib/interpreter.h"
#include "../groot_lib/properties.h"
#include "docopt/docopt.h"
#include <boost/filesystem.hpp>
#include <filesystem>


using namespace std;
using json = nlohmann::json;

template <class EdgeMap>
class edge_writer {
public:
	edge_writer(EdgeMap w) : wm(w) {}
	template <class Edge>
	void operator()(ostream& out, const Edge& e) const {
		auto type = get(wm, e);
		if (type == normal) {
			out << "[color=black]";
		}
		else {
			out << "[color=red]";
		}
	}
private:
	EdgeMap wm;
};

template <class EdgeMap>
inline edge_writer<EdgeMap>
make_edge_writer(EdgeMap w) {
	return edge_writer<EdgeMap>(w);
}

template <class VertexMap>
class vertex_writer {
public:
	vertex_writer(VertexMap w) : wm(w) {}
	template <class Vertex>
	void operator()(ostream& out, const Vertex& v) const {
		auto type = get(wm, v);
		out << "[label=\"" << type.get() << "\"]";
	}
private:
	VertexMap wm;
};

template <class VertexMap>
inline vertex_writer<VertexMap>
make_vertex_writer(VertexMap w) {
	return vertex_writer<VertexMap>(w);
}

inline char const* TypeToString(EdgeType c) {
	switch (c) {
	case EdgeType::normal:  return "black";
	case EdgeType::dname:   return "red";
	}
	return ""; // not known
}

template<class T>
void serialize(T& data, string fileName) {
	std::ofstream outputfile{ fileName };
	boost::archive::text_oarchive oa{ outputfile };
	oa << data;
}

template<class T>
void deserialize(T& data, string fileName) {
	std::ifstream file1{ fileName };
	boost::archive::text_iarchive ia(file1);
	ia >> data;
}

std::bitset<RRType::N> ProcessProperties(json j, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>>& pathFunctions) {
	std::bitset<RRType::N> typesReq;
	for (auto& property : j) {
		string name = property["PropertyName"];
		std::bitset<RRType::N> propertyTypes;
		if (property.find("Types") != property.end()) {
			for (auto typ : property["Types"]) {
				typesReq.set(ConvertToRRType(typ));
				propertyTypes.set(ConvertToRRType(typ));
			}
		}
		else {
			typesReq.set(RRType::NS);
		}

		if (name == "ResponseConsistency") {
			auto la = [types = std::move(propertyTypes)](const InterpreterGraph & graph, const Path & p){ CheckSameResponseReturned(graph, p, types); };
			nodeFunctions.push_back(la);
		}
		else if (name == "ResponseReturned") {
			auto la = [types = std::move(propertyTypes)](const InterpreterGraph & graph, const Path & p){ CheckResponseReturned(graph, p, types); };
			nodeFunctions.push_back(la);
		}
		else if (name == "ResponseValue") {
			vector<string> values;
			for (auto v : property["Value"]) {
				values.push_back(v);
			}
			auto la = [types = std::move(propertyTypes), v = std::move(values)](const InterpreterGraph & graph, const Path & p){ CheckResponseValue(graph, p, types, v); };
			nodeFunctions.push_back(la);
		}
		else if (name == "Hops") {
			auto l = [num_hops = property["Value"]](const InterpreterGraph & graph, const Path & p) {NumberOfHops(graph, p, num_hops); };
			pathFunctions.push_back(l);
		}
		else if (name == "Rewrites") {
			auto l = [num_rewrites = property["Value"]](const InterpreterGraph & graph, const Path & p) {NumberOfRewrites(graph, p, num_rewrites); };
			pathFunctions.push_back(l);
		}
		else if (name == "DelegationConsistency") {
			pathFunctions.push_back(CheckDelegationConsistency);
		}
	}
	return typesReq;
}

void demo(string directory, string properties) {
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set(".");
	std::ifstream metadataFile((boost::filesystem::path{ directory } / boost::filesystem::path{ "metadata.json" }).string());
	json metadata;
	metadataFile >> metadata;
	for (auto& server : metadata["TopNameServers"]) {
		gTopNameServers.push_back(server);
	}
	for (auto& zone : metadata["ZoneFiles"]) {
		string filename; 
		zone["FileName"].get_to(filename);
		auto zoneFilePath = (boost::filesystem::path{ directory } / boost::filesystem::path{ filename }).string();
		BuildZoneLabelGraphs(zoneFilePath, zone["NameServer"], g, root);
	}
	/*std::ofstream dotfile("LabelGraph.dot");	
	write_graphviz(dotfile, g, make_vertex_writer(boost::get(&LabelVertex::name, g)), make_edge_writer(boost::get(&LabelEdge::type, g)));*/
	std::ifstream i(properties);
	json j;
	i >> j;
	vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> nodeFunctions;
	vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions;
	for (auto& query : j) {
		nodeFunctions.clear();
		pathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], nodeFunctions, pathFunctions);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], nodeFunctions, pathFunctions);
		cout << endl;
	}
}


void bench(string directory, string input) {
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set(".");
	gTopNameServers.push_back(".");
	BuildZoneLabelGraphs(directory + "root.txt", ".", g, root);
	std::ifstream i(input);
	json j;
	i >> j;
	vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> nodeFunctions;
	vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions;
	for (auto& query : j) {
		nodeFunctions.clear();
		pathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], nodeFunctions, pathFunctions);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], nodeFunctions, pathFunctions);
		cout << endl;
	}
}

	
void checkHotmailDomains(string directory, string properties) {
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set("");
	//auto zoneFilePath = (boost::filesystem::path{ "C:\\Users\\Administrator\\Desktop\\groot\\zone_files" } / boost::filesystem::path{ "hotmail.com" } / boost::filesystem::path{ "hotmail.com - Copy.dns" }).string();
	gTopNameServers.push_back("ns1.msft.net.");
	for (auto& entry : filesystem::directory_iterator(directory)) {
		BuildZoneLabelGraphs(entry.path().string(), "ns1.msft.com", g, root);
	}	
	/*std::ofstream dotfile("LabelGraph.dot");
	write_graphviz(dotfile, g, make_vertex_writer(boost::get(&LabelVertex::name, g)), make_edge_writer(boost::get(&LabelEdge::type, g)));
	*/
	//CheckStructuralDelegationConsistency(g, root, "bay003.hotmail.com.", {});
	CheckAllStructuralDelegations(g, root, "", root);
	/*std::ifstream i(properties);
	json j;
	i >> j;
	vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>> nodeFunctions;
	vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions;
	for (auto& query : j) {
		nodeFunctions.clear();
		pathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], nodeFunctions, pathFunctions);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], nodeFunctions, pathFunctions);
		cout << endl;
	}*/
}

void checkUCLADomains(string directory, string properties) {
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set("");
	gTopNameServers.push_back("ns1.dns.ucla.edu.");
	for (auto& entry : filesystem::directory_iterator(directory)) {
		BuildZoneLabelGraphs(entry.path().string(), "ns1.dns.ucla.edu.", g, root);
	}
	//CheckStructuralDelegationConsistency(g, root, "cs.ucla.edu.", {});
	CheckAllStructuralDelegations(g, root, "", root);
}

static const char USAGE[] =
R"(groot 1.0
   
Groot is a static verification tool for DNS. Groot consumes
a collection of zone files along with a collection of user- 
defined properties and systematically checks if any input to
DNS can lead to a property violation for the properties.

Usage: groot [-hdv] [--properties=<properties_file>] <zone_directory>

Options:
  -h --help     Show this help screen.
  --version     Show groot version.
  -d --debug    Generate debugging dot files.
  -v --verbose  Print more information.  
)";


int main(int argc, const char** argv)
{
	auto args = docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "groot 1.0");
	
	/* for (auto const& arg : args) {
		std::cout << arg.first << arg.second << std::endl;
	 }*/

	string zone_directory;
	string properties_file;
	
	auto z = args.find("<zone_directory>");
	if (!z->second)
	{
		cout << "Error: missing parameter <zone_directory>" << endl;
		cout << USAGE[0];
		exit(0);
	}
	else
	{
		zone_directory = z->second.asString();
	}

	auto p = args.find("--properties");
	if (p->second)
	{
		properties_file = p->second.asString();
	}

	bool verbose = args.find("--verbose")->second.asBool();
	bool debug_dot = args.find("--debug")->second.asBool();

	// TODO: validate that the directory and property files exist

	//profiling_net();
	//bench(zone_directory, properties_file);
	checkHotmailDomains(zone_directory, properties_file);
	//checkUCLADomains(zone_directory, properties_file);
	//demo(zone_directory, properties_file);

	return 0;
}