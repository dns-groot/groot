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

void propertySelector(vector<int>& indices, vector<EC>& allQueries) {
	cout << "\nEnter a comma(,) separated list of integers to check the following properties" << endl;
	cout << "1: Some reponse returned" << endl;
	cout << "2: Same response returned" << endl;
	cout << "3: Number of rewrites " << endl;
	cout << "4: Number of hops" << endl;
	string userInput;

	vector<std::function<void(InterpreterGraph&, vector<InterpreterVertexDescriptor>&, std::bitset<RRType::N>)>> nodeFunctions;
	vector<std::function<void(InterpreterGraph&, Path&)>> pathFunctions;
	while (true) {
		cin >> userInput;
		vector<std::string> labels;
		boost::algorithm::split(labels, userInput, boost::is_any_of(","));
		bool found = false;
		for (auto i : labels) {
			int s = std::stoi(i);
			if (s > 5 || s < 0) {
				cout << "Enter comma separated list of integers or 0 to exit" << endl;
				break;
			}
			else {
				found = true;
				if (s == 0) {
					return;
				}
				if (s == 1) {
					nodeFunctions.push_back(CheckResponseReturned);
				}
				if (s == 2) {
					nodeFunctions.push_back(CheckSameResponseReturned);
				}
				if (s == 3) {
					cout << " Enter the maximum number of rewrites allowed" << endl;
					int num_rewrites;
					cin >> num_rewrites;
					auto l = [num_rewrites = std::move(num_rewrites)](InterpreterGraph & graph, Path & p) {NumberOfRewrites(graph, p, num_rewrites); };
					pathFunctions.push_back(l);
				}
				if (s == 4) {
					cout << " Enter the maximum number of hops allowed" << endl;
					int num_hops;
					cin >> num_hops;
					auto l = [num_hops = std::move(num_hops)](InterpreterGraph & graph, Path & p) {NumberOfHops(graph, p, num_hops); };
					pathFunctions.push_back(l);
				}
			}
		}
		if (found) {
			break;
		}
	}

	//for (auto& i : indices) {
	//	InterpreterGraphWrapper intGraphWrapper;
	//	BuildInterpretationGraph(allQueries[i], intGraphWrapper);
	//	vector<InterpreterVertexDescriptor> endNodes;
	//	CheckProperties(intGraphWrapper, intGraphWrapper.intG[intGraphWrapper.startVertex].query.rrTypes, nodeFunctions, pathFunctions);
	//}
}


void selector(LabelGraph& g, vector<EC>& allQueries) {
	cout << "Enter an integer from 1-3 to choose one of the following for the input zone files to check for properties:" << endl;
	cout << "1: All possible queries" << endl;
	cout << "2: Some sub-domain" << endl;
	cout << "3: Particular domain" << endl;
	int i = -1;
	while (i < 0 || i > 3) {
		cin >> i;
		if (i < 0 || i > 3) {
			cout << "Enter 0 to exit or an integer from 1-3" << endl;
		}
	}
	string domain;
	switch (i)
	{
	case 1:
		break;
	case 2:
		cout << "Enter the parent domain" << endl;
		cin >> domain;
		//domain_to_EC(domain, g, true);
		break;
	case 3: {
		cout << "Enter the domain name" << endl;
		/*cin >> domain;
		std::vector<int> relevant = DomainToEC(domain, g, false);
		if (relevant.size() == 0) {
			cout << "The domain entered doesn't exist in the input zone files"<<endl;
		}
		else {
			propertySelector(relevant, allQueries);
		}
		break;*/
	}
	default:
		return;
	}
}

std::bitset<RRType::N> ProcessProperties(json j, vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>>& pathFunctions) {
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

void demo(string directory, string input) {
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set(".");
	gTopNameServers.push_back("ns1.tld.sy.");
	BuildZoneLabelGraphs(directory + "net.sy.txt", "ns1.tld.sy.", g, root, gNameServerZoneMap);
	BuildZoneLabelGraphs(directory + "mtn.net.sy.txt", "ns1.mtn.net.sy.", g, root, gNameServerZoneMap);
	BuildZoneLabelGraphs(directory + "child.mtn.net.sy.txt", "ns1.child.mtn.net.sy.", g, root, gNameServerZoneMap);
	BuildZoneLabelGraphs(directory + "child.mtn.net.sy-2.txt", "ns2.child.mtn.net.sy.", g, root, gNameServerZoneMap);

	std::ofstream dotfile("LabelGraph.dot");
	write_graphviz(dotfile, g, make_vertex_writer(boost::get(&LabelVertex::name, g)), make_edge_writer(boost::get(&LabelEdge::type, g)));

	std::ifstream i(input);
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
	}
}


void bench(string directory, string input) {
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set(".");
	gTopNameServers.push_back(".");
	BuildZoneLabelGraphs(directory + "root.txt", ".", g, root, gNameServerZoneMap);
	std::ifstream i(input);
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
	}
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
	
	// for (auto const& arg : args) {
	//	std::cout << arg.first << arg.second << std::endl;
	// }

	string zone_directory;
	string properties_file;
	
	auto z = args.find("<zone_directory>");
	if (z == args.end())
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
	if (p != args.end())
	{
		properties_file = p->second.asString();
	}

	bool verbose = args.find("--verbose")->second.asBool();
	bool debug_dot = args.find("--debug")->second.asBool();

	// TODO: validate that the directory and property files exist

	//profiling_net();
	//bench(zone_directory, properties_file);
	demo(zone_directory, properties_file);

	return 0;
}