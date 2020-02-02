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
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <filesystem>
#include <ctime>
#include <ratio>
#include <chrono>

using namespace std;
using json = nlohmann::json;
using namespace std::chrono;

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

std::bitset<RRType::N> ProcessProperties(json j, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>>& pathFunctions, json& output) {
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
			auto la = [types = std::move(propertyTypes), &output](const InterpreterGraph& graph, const Path& p){ CheckSameResponseReturned(graph, p, types, output); };
			nodeFunctions.push_back(la);
		}
		else if (name == "ResponseReturned") {
			auto la = [types = std::move(propertyTypes), &output](const InterpreterGraph& graph, const Path& p){ CheckResponseReturned(graph, p, types, output); };
			nodeFunctions.push_back(la);
		}
		else if (name == "ResponseValue") {
			std::set<string> values;
			for (string v : property["Value"]) {
				values.insert(v);
			}
			auto la = [types = std::move(propertyTypes), v = std::move(values), &output](const InterpreterGraph& graph, const Path& p){ CheckResponseValue(graph, p, types, v, output); };
			nodeFunctions.push_back(la);
		}
		else if (name == "Hops") {
			auto l = [num_hops = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {NumberOfHops(graph, p, num_hops, output); };
			pathFunctions.push_back(l);
		}
		else if (name == "Rewrites") {
			auto l = [num_rewrites = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {NumberOfRewrites(graph, p, num_rewrites, output); };
			pathFunctions.push_back(l);
		}
		else if (name == "DelegationConsistency") {
			auto l = [&output](const InterpreterGraph& graph, const Path& p) {CheckDelegationConsistency(graph, p, output); };
			pathFunctions.push_back(l);
		}
		else if (name == "LameDelegation") {
			auto l = [&output](const InterpreterGraph& graph, const Path& p) {CheckLameDelegation(graph, p, output); };
			pathFunctions.push_back(l);
		}
		else if (name == "QueryRewrite") {
			auto l = [d = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {QueryRewrite(graph, p, GetLabels(d), output); };
			pathFunctions.push_back(l);
		}
		else if (name == "NameServerContact") {
			auto l = [d = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {NameServerContact(graph, p, GetLabels(d), output); };
			pathFunctions.push_back(l);
		}
	}
	return typesReq;
}

void demo(string directory, string properties, json& output) {
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
	std::ofstream dotfile("LabelGraph.dot");
	write_graphviz(dotfile, g, make_vertex_writer(boost::get(&LabelVertex::name, g)), make_edge_writer(boost::get(&LabelEdge::type, g)));
	std::ifstream i(properties);
	json j;
	i >> j;
	vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> nodeFunctions;
	vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions;
	for (auto& query : j) {
		nodeFunctions.clear();
		pathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], nodeFunctions, pathFunctions, output);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], nodeFunctions, pathFunctions, output);
	}
}


void bench(string directory, string input, json& output) {
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
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], nodeFunctions, pathFunctions, output);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], nodeFunctions, pathFunctions, output);
		cout << endl;
	}
}


void checkHotmailDomains(string directory, string properties, json& output) {
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
	CheckAllStructuralDelegations(g, root, "", root, output);
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

void checkUCLADomains(string directory, string properties, json& output) {

	gECcount = 0;
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	LabelGraph g;
	VertexDescriptor root = boost::add_vertex(g);
	g[root].name.set("");
	gTopNameServers.push_back("ns1.ucla.edu");
	for (auto& entry : filesystem::directory_iterator(directory)) {
		if (entry.path().string() == directory + "\\ucla.edu_") {
			BuildZoneLabelGraphs(entry.path().string(), "ns1.ucla.edu", g, root);
		}
		else {
			BuildZoneLabelGraphs(entry.path().string(), "ns1.dns.ucla.edu.", g, root);
		}	
	}
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
	cout << " Time took for Label graph and zone graphs" << time_span.count() << endl;
	std::ifstream i(properties);
	json j;
	i >> j;
	vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> nodeFunctions;
	vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions;
	for (auto& query : j) {
		nodeFunctions.clear();
		pathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], nodeFunctions, pathFunctions, output);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], nodeFunctions, pathFunctions, output);
		cout << endl;
	}
	//CheckStructuralDelegationConsistency(g, root, "aap.ucla.edu.", {}, output);
	CheckAllStructuralDelegations(g, root, "", root, output);
	t2 = high_resolution_clock::now();
	time_span = duration_cast<duration<double>>(t2 - t1);
	cout << " Time to check properties" << time_span.count() << endl;
	cout << " Total Number of ECs" << gECcount << endl;
}

inline bool file_exists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void ZoneFileNSMap(string file, std::map<string, string>& zoneFileNameToNS) {
	std::ifstream infile(file);
	std::string line;
	const boost::regex fieldsregx(",(?=(?:[^\"]*\"[^\"]*\")*(?![^\"]*\"))");
	const boost::regex linesregx("\\r\\n|\\n\\r|\\n|\\r");
	while (std::getline(infile, line))
	{
		boost::sregex_token_iterator ti(line.begin(), line.end(), fieldsregx, -1);
		boost::sregex_token_iterator end2;

		std::vector<std::string> row;
		while (ti != end2) {
			std::string token = ti->str();
			++ti;
			row.push_back(token);
		}
		if (line.back() == ',') {
			// last character was a separator
			row.push_back("");
		}
		zoneFileNameToNS.insert(std::pair<string, string>(row[2], row[1]));
	}
}


void CensusProperties(string domain, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& nodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>>& pathFunctions, json& output) {
			
	auto l = [d = domain, &output](const InterpreterGraph& graph, const Path& p) {QueryRewrite(graph, p, GetLabels(d), output); };
	pathFunctions.push_back(l);
	auto name = [d = domain, &output](const InterpreterGraph& graph, const Path& p) {NameServerContact(graph, p, GetLabels(d), output); };
	pathFunctions.push_back(name);
	auto re = [num_rewrites = 3, &output](const InterpreterGraph& graph, const Path& p) {NumberOfRewrites(graph, p, num_rewrites, output); };
	pathFunctions.push_back(re);
}

void DNSCensus(string zoneFilesdirectory, string zoneNS, string tldSubDomainMap, string outputDirectory) {
	std::ifstream i(tldSubDomainMap);
	json tldMap;
	i >> tldMap;
	std::map<string, string> zoneFileNameToNS;
	ZoneFileNSMap(zoneNS, zoneFileNameToNS);

	for (auto& [key, value] : tldMap.items()) {
		if (value.size() < 100) {
			gECcount = 0;
			string fileName = key + "..txt";
			auto zoneFilePath = (boost::filesystem::path{ zoneFilesdirectory } / boost::filesystem::path{ fileName }).string();
			//check if the tld file exists
			if (file_exists(zoneFilePath)) {
				auto it = zoneFileNameToNS.find(fileName);
				//check if the tld file to name server map exists
				if (it != zoneFileNameToNS.end())
				{
					gTopNameServers.clear();
					gZoneIdToZoneMap.clear();
					gNameServerZoneMap.clear();
					high_resolution_clock::time_point t1 = high_resolution_clock::now();
					LabelGraph g;
					VertexDescriptor root = boost::add_vertex(g);
					g[root].name.set("");
					cout << " Processing " + key << endl;
					gTopNameServers.push_back(it->second);
					BuildZoneLabelGraphs(zoneFilePath, it->second, g, root);
					//process subdomains
					for (auto& subdomain : value) {
						fileName = subdomain + "..txt";
						zoneFilePath = (boost::filesystem::path{ zoneFilesdirectory } / boost::filesystem::path{ fileName }).string();
						if (file_exists(zoneFilePath)) {
							auto itsub = zoneFileNameToNS.find(fileName);
							if (itsub != zoneFileNameToNS.end()) {
								BuildZoneLabelGraphs(zoneFilePath, itsub->second, g, root);
							}
						}
					}
					high_resolution_clock::time_point t2 = high_resolution_clock::now();
					duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
					json output = json::array();
					vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> nodeFunctions;
					vector<std::function<void(const InterpreterGraph&, const Path&)>> pathFunctions;
					CensusProperties(key, nodeFunctions, pathFunctions, output);
					std::bitset<RRType::N> typesReq;
					typesReq.set(RRType::NS);
					GenerateECAndCheckProperties(g, root, key, typesReq, true, nodeFunctions, pathFunctions, output);
					high_resolution_clock::time_point t3 = high_resolution_clock::now();
					duration<double> time_span_EC = duration_cast<duration<double>>(t3 - t2);
					json filteredOutput = json::array();
					filteredOutput["Differences"] = {};
					for (json j : output) {
						bool found = false;
						for (json l : filteredOutput["Differences"]) {
							if (l == j) {
								found = true;
								break;
							}
						}
						if (!found) filteredOutput["Differences"].push_back(j);
					}
					filteredOutput["ECs"] = gECcount;
					filteredOutput["graph building"] = time_span.count();
					filteredOutput["property checking"] = time_span_EC.count();
					std::ofstream ofs;
					auto outputFile = (boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ key+".json" }).string();
					ofs.open(outputFile, std::ofstream::out);
					ofs << filteredOutput.dump(4);
					ofs << "\n";
					ofs.close();
				}
			}
		}
	}
	exit(0);
}

static const char USAGE[] =
R"(groot 1.0
   
Groot is a static verification tool for DNS. Groot consumes
a collection of zone files along with a collection of user- 
defined properties and systematically checks if any input to
DNS can lead to a property violation for the properties.

Usage: groot [-h] [--properties=<properties_file>] <zone_directory> [--output=<output_file>]

Options:
  -h --help     Show this help screen.
  --version     Show groot version.
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
	json output = json::array();
	//profiling_net();
	//bench(zone_directory, properties_file);
	//checkHotmailDomains(zone_directory, properties_file, output);
	checkUCLADomains(zone_directory, properties_file, output);
	//demo(zone_directory, properties_file, output);
	json filteredOutput = json::array();
	for (json j : output) {
		bool found = false;
		for (json l : filteredOutput) {
			if (l == j) {
				found = true;
				break;
			}
		}
		if (!found) filteredOutput.push_back(j);
	}
	
	p = args.find("--output");
	string outputFile = "output.json";
	if (p->second)
	{
		outputFile = p->second.asString();
	}
	std::ofstream ofs;
	ofs.open(outputFile, std::ofstream::out);
	ofs << filteredOutput.dump(4);
	ofs << "\n";
	ofs.close();
	return 0;
}