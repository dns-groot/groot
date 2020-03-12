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
#include "../groot_lib/interpreter.h"
#include "../groot_lib/properties.h"
#include "docopt/docopt.h"
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <filesystem>
#include <ctime>
#include <ratio>
#include <chrono>

//#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

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

std::bitset<RRType::N> ProcessProperties(json properties, json& output) {
	std::bitset<RRType::N> typesReq;
	for (auto& property : properties) {
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
			auto la = [types = std::move(propertyTypes), &output](const InterpreterGraph& graph, const Path& p){ CheckSameResponseReturned(graph, p, types); };
			gNodeFunctions.push_back(la);
		}
		else if (name == "ResponseReturned") {
			auto la = [types = std::move(propertyTypes), &output](const InterpreterGraph& graph, const Path& p){ CheckResponseReturned(graph, p, types); };
			gNodeFunctions.push_back(la);
		}
		else if (name == "ResponseValue") {
			std::set<string> values;
			for (string v : property["Value"]) {
				values.insert(v);
			}
			auto la = [types = std::move(propertyTypes), v = std::move(values), &output](const InterpreterGraph& graph, const Path& p){ CheckResponseValue(graph, p, types, v); };
			gNodeFunctions.push_back(la);
		}
		else if (name == "Hops") {
			auto l = [num_hops = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {NumberOfHops(graph, p, num_hops); };
			gPathFunctions.push_back(l);
		}
		else if (name == "Rewrites") {
			auto l = [num_rewrites = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {NumberOfRewrites(graph, p, num_rewrites); };
			gPathFunctions.push_back(l);
		}
		else if (name == "DelegationConsistency") {
			auto l = [&output](const InterpreterGraph& graph, const Path& p) {CheckDelegationConsistency(graph, p); };
			gPathFunctions.push_back(l);
		}
		else if (name == "LameDelegation") {
			auto l = [&output](const InterpreterGraph& graph, const Path& p) {CheckLameDelegation(graph, p); };
			gPathFunctions.push_back(l);
		}
		else if (name == "QueryRewrite") {
			auto l = [d = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {QueryRewrite(graph, p, GetLabels(d)); };
			gPathFunctions.push_back(l);
		}
		else if (name == "NameServerContact") {
			auto l = [d = property["Value"], &output](const InterpreterGraph& graph, const Path& p) {NameServerContact(graph, p, GetLabels(d)); };
			gPathFunctions.push_back(l);
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
	Logger->debug("groot.cpp (demo) - Successfully read metadata.json file");
	std::ifstream i(properties);
	json j;
	i >> j;
	Logger->debug("groot.cpp (demo) - Successfully read properties.json file");
	for (auto& server : metadata["TopNameServers"]) {
		gTopNameServers.push_back(server);
	}
	for (auto& zone : metadata["ZoneFiles"]) {
		string filename;
		zone["FileName"].get_to(filename);
		auto zoneFilePath = (boost::filesystem::path{ directory } / boost::filesystem::path{ filename }).string();
		BuildZoneLabelGraphs(zoneFilePath, zone["NameServer"], g, root);
	}
	Logger->debug("groot.cpp (demo) - Label graph and Zone graphs built");
	/*std::ofstream dotfile("LabelGraph.dot");
	write_graphviz(dotfile, g, make_vertex_writer(boost::get(&LabelVertex::name, g)), make_edge_writer(boost::get(&LabelEdge::type, g)));*/
	for (auto& query : j) {
		gNodeFunctions.clear();
		gPathFunctions.clear();
		gDoneECgeneration = false;
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], output);
		Logger->debug(fmt::format("groot.cpp (demo) - Started property checking for {}", string(query["Domain"])));
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], output);
		Logger->debug(fmt::format("groot.cpp (demo) - Finished property checking for {}",string(query["Domain"])));
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
	for (auto& query : j) {
		gNodeFunctions.clear();
		gPathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], output);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], output);
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
	CheckAllStructuralDelegations(g, root, "", root); // Need to pop out from the queue
	/*std::ifstream i(properties);
	json j;
	i >> j;
	vector<std::function<void(const InterpreterGraph&, const vector<InterpreterVertexDescriptor>&)>> gNodeFunctions;
	vector<std::function<void(const InterpreterGraph&, const Path&)>> gPathFunctions;
	for (auto& query : j) {
		gNodeFunctions.clear();
		gPathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], gNodeFunctions, gPathFunctions);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], gNodeFunctions, gPathFunctions);
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
	cout << "Time to build label graph and zone graphs: " << time_span.count() << endl;
	std::ifstream i(properties);
	json j;
	i >> j;
	for (auto& query : j) {
		gNodeFunctions.clear();
		gPathFunctions.clear();
		std::bitset<RRType::N> typesReq = ProcessProperties(query["Properties"], output);
		GenerateECAndCheckProperties(g, root, query["Domain"], typesReq, query["SubDomain"], output);
	}
	//CheckStructuralDelegationConsistency(g, root, "aap.ucla.edu.", {}, output);
	//CheckAllStructuralDelegations(g, root, "", root, output);
	t2 = high_resolution_clock::now();
	time_span = duration_cast<duration<double>>(t2 - t1);
	cout << "Time to check properties: " << time_span.count() << endl;
	cout << "Total Number of ECs: " << gECcount << endl;
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


void CensusProperties(string domain, vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>>& gNodeFunctions, vector<std::function<void(const InterpreterGraph&, const Path&)>>& gPathFunctions, json& output) {

	auto l = [d = domain, &output](const InterpreterGraph& graph, const Path& p) {QueryRewrite(graph, p, GetLabels(d)); };
	gPathFunctions.push_back(l);
	auto name = [d = domain, &output](const InterpreterGraph& graph, const Path& p) {NameServerContact(graph, p, GetLabels(d)); };
	gPathFunctions.push_back(name);
	auto re = [num_rewrites = 3, &output](const InterpreterGraph& graph, const Path& p) {NumberOfRewrites(graph, p, num_rewrites); };
	gPathFunctions.push_back(re);
}

void DNSCensus(string zoneFilesdirectory, string zoneNS, string tldSubDomainMap, string outputDirectory) {
	std::ifstream i(tldSubDomainMap);
	json tldMap;
	i >> tldMap;
	std::map<string, string> zoneFileNameToNS;
	ZoneFileNSMap(zoneNS, zoneFileNameToNS);
	cout << "Zone File to NS map generated" << endl;
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
						string subd = subdomain.get<string>();
						fileName = subd + "..txt";
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
					vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> gNodeFunctions;
					vector<std::function<void(const InterpreterGraph&, const Path&)>> gPathFunctions;
					CensusProperties(key, gNodeFunctions, gPathFunctions, output);
					std::bitset<RRType::N> typesReq;
					typesReq.set(RRType::NS);
					GenerateECAndCheckProperties(g, root, key, typesReq, true, output);
					high_resolution_clock::time_point t3 = high_resolution_clock::now();
					duration<double> time_span_EC = duration_cast<duration<double>>(t3 - t2);
					json filteredOutput;
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
					filteredOutput["ECs"] = gECcount.load();
					filteredOutput["graph building"] = time_span.count();
					filteredOutput["property checking"] = time_span_EC.count();
					std::ofstream ofs;
					auto outputFile = (boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ key + ".json" }).string();
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


void debug() {

	json subdomains;
	json nsMap;
	gECcount = 0;
	// This one generates a huge graph with infinite loops kind
	/*string domain = "2020.net";
	string fileName = domain + "..txt";
	subdomains.push_back("wan.2020.net");
	subdomains.push_back("ho.wan.2020.net");
	nsMap["2020.net..txt"] = "dns1.2020.net.";
	nsMap["wan.2020.net..txt"] = "srv-dcroot.wan.2020.net.";
	nsMap["ho.wan.2020.net..txt"] = "srv-dcroot.wan.2020.net.";*/

	//The following input would not work as the parenthesis throws and derails the parser
	string domain = "bigcal.org";
	string fileName = domain + "..txt";
	subdomains.push_back("www.bigcal.org");
	nsMap["bigcal.org..txt"] = "\\(4110125364.";
	nsMap["www.bigcal.org..txt"] = "\\(181576594.";


	auto zoneFilePath = (boost::filesystem::path{ "E:\\siva\\DNSCensus2013\\FullZones" } / boost::filesystem::path{ fileName }).string();
	if (file_exists(zoneFilePath)) {
		auto it = nsMap.find(fileName);
		//check if the tld file to name server map exists
		if (it != nsMap.end())
		{
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			LabelGraph g;
			VertexDescriptor root = boost::add_vertex(g);
			g[root].name.set("");
			gTopNameServers.push_back(nsMap[fileName]);
			BuildZoneLabelGraphs(zoneFilePath, nsMap[fileName], g, root);
			//process subdomains
			for (auto& subdomain : subdomains) {
				string subd = subdomain.get<string>();
				subd = subd + "..txt";
				zoneFilePath = (boost::filesystem::path{ "E:\\siva\\DNSCensus2013\\FullZones" } / boost::filesystem::path{ subd }).string();
				if (file_exists(zoneFilePath)) {
					auto itsub = nsMap.find(subd);
					if (itsub != nsMap.end()) {
						BuildZoneLabelGraphs(zoneFilePath, nsMap[subd], g, root);
					}
				}
			}
			high_resolution_clock::time_point t2 = high_resolution_clock::now();
			duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
			json output = json::array();
			vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> gNodeFunctions;
			vector<std::function<void(const InterpreterGraph&, const Path&)>> gPathFunctions;
			CensusProperties(string(domain), gNodeFunctions, gPathFunctions, output);
			std::bitset<RRType::N> typesReq;
			typesReq.set(RRType::NS);
			GenerateECAndCheckProperties(g, root, string(domain), typesReq, true, output);
			high_resolution_clock::time_point t3 = high_resolution_clock::now();
			duration<double> time_span_EC = duration_cast<duration<double>>(t3 - t2);
			json filteredOutput;
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
			filteredOutput["ECs"] = gECcount.load();
			filteredOutput["graph building"] = time_span.count();
			filteredOutput["property checking"] = time_span_EC.count();
			std::ofstream ofs;
			auto outputFile = (boost::filesystem::path{ string("E:\\siva\\SecondLevelOutputs - Copy\\") } / boost::filesystem::path{ string(domain) + ".json" }).string();
			ofs.open(outputFile, std::ofstream::out);
			ofs << filteredOutput.dump(4);
			ofs << "\n";
			ofs.close();
		}
	}
	exit(EXIT_SUCCESS);
}


//int main(int argc, const char* argv[])
//{
//	/*
//	1 - zoneFiles directory
//	2 - domain name
//	3 - list of subdomains
//	4 - filename to name server mapping
//	5 - output directory
//	
//	*/
//	//debug();
//	/*json subdomains = json::parse(argv[3]);
//	json nsMap = json::parse(argv[4]);*/
//	std::ifstream i(argv[3]);
//	json subdomains;
//	i >> subdomains;
//	std::ifstream k(argv[4]);
//	json nsMap;
//	k >> nsMap;
//
//	gECcount = 0;
//	string fileName = string(argv[2]) + "..txt";
//	auto zoneFilePath = (boost::filesystem::path{ argv[1] } / boost::filesystem::path{ fileName }).string();
//	//check if the tld file exists
//	if (file_exists(zoneFilePath)) {
//		auto it = nsMap.find(fileName);
//		//check if the tld file to name server map exists
//		if (it != nsMap.end())
//		{
//			/*cout << "Inside " << endl;*/
//			high_resolution_clock::time_point t1 = high_resolution_clock::now();
//			LabelGraph g;
//			VertexDescriptor root = boost::add_vertex(g);
//			g[root].name.set("");
//			gTopNameServers.push_back(nsMap[fileName]);
//			BuildZoneLabelGraphs(zoneFilePath, nsMap[fileName], g, root);
//			//process subdomains
//			for (auto& subdomain : subdomains) {
//				string subd = subdomain.get<string>();
//				subd = subd + "..txt";
//				zoneFilePath = (boost::filesystem::path{ argv[1] } / boost::filesystem::path{ subd }).string();
//				if (file_exists(zoneFilePath)) {
//					auto itsub = nsMap.find(subd);
//					if (itsub != nsMap.end()) {
//						BuildZoneLabelGraphs(zoneFilePath, nsMap[subd], g, root);
//					}
//				}
//			}
//			high_resolution_clock::time_point t2 = high_resolution_clock::now();
//			duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
//			json output = json::array();
//			vector<std::function<void(const InterpreterGraph&, const vector<IntpVD>&)>> gNodeFunctions;
//			vector<std::function<void(const InterpreterGraph&, const Path&)>> gPathFunctions;
//			CensusProperties(string(argv[2]), gNodeFunctions, gPathFunctions, output);
//			std::bitset<RRType::N> typesReq;
//			typesReq.set(RRType::NS);
//			GenerateECAndCheckProperties(g, root, string(argv[2]), typesReq, true, gNodeFunctions, gPathFunctions, output);
//			high_resolution_clock::time_point t3 = high_resolution_clock::now();
//			duration<double> time_span_EC = duration_cast<duration<double>>(t3 - t2);
//			json filteredOutput;
//			/*filteredOutput["Differences"] = {};
//			for (json j : output) {
//				bool found = false;
//				for (json l : filteredOutput["Differences"]) {
//					if (l == j) {
//						found = true;
//						break;
//					}
//				}
//				if (!found) filteredOutput["Differences"].push_back(j);
//			}*/
//			filteredOutput["ECs"] = gECcount;
//			filteredOutput["graph building"] = time_span.count();
//			filteredOutput["property checking"] = time_span_EC.count();
//			std::ofstream ofs;
//			auto outputFile = (boost::filesystem::path{ string(argv[5]) } / boost::filesystem::path{ string(argv[2]) + ".json" }).string();
//			ofs.open(outputFile, std::ofstream::out);
//			ofs << filteredOutput.dump(4);
//			ofs << "\n";
//			ofs.close();
//		}
//	}
//	return 0;
//}

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
	try {
		auto args = docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "groot 1.0");

		/* for (auto const& arg : args) {
			std::cout << arg.first << arg.second << std::endl;
		 }*/
		spdlog::init_thread_pool(8192, 1);
		
		auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		stdout_sink->set_level(spdlog::level::err);
		stdout_sink->set_pattern("[%x %H:%M:%S.%e] [thread %t] [%^%=7l%$] %v");
		
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);
		file_sink->set_level(spdlog::level::trace);
		file_sink->set_pattern("[%x %H:%M:%S.%e] [thread %t] [%^%=7l%$] %v");
		
		std::vector<spdlog::sink_ptr> sinks{ stdout_sink, file_sink };
		auto logger = std::make_shared<spdlog::async_logger>("my_custom_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		//auto  logger = std::make_shared<spdlog::logger>("my_custom_logger", sinks.begin(), sinks.end());
		logger->flush_on(spdlog::level::trace);
		logger->set_level(spdlog::level::trace);

		spdlog::register_logger(logger);
		
		Logger->bind(spdlog::get("my_custom_logger"));

		string zone_directory;
		string properties_file;
		auto z = args.find("<zone_directory>");
		if (!z->second)
		{
			Logger->critical(fmt::format("groot.cpp (main) - missing parameter <zone_directory>"));
			cout << USAGE[0];
			exit(EXIT_FAILURE);
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

		/*bool verbose = args.find("--verbose")->second.asBool();
		bool debug_dot = args.find("--debug")->second.asBool();*/

		// TODO: validate that the directory and property files exist
		json output = json::array();
		//profiling_net();
		//bench(zone_directory, properties_file);
		//checkHotmailDomains(zone_directory, properties_file, output);
		//checkUCLADomains(zone_directory, properties_file, output);
		demo(zone_directory, properties_file, output);
		Logger->debug("groot.cpp (main) - Finished checking properties");
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
		Logger->debug(fmt::format("groot.cpp (main) - Output written to {}", outputFile));
		spdlog::shutdown();	
		return 0;
	}
	catch (exception & e) {
		cout << "Exception:- " << e.what() << endl;
	}
}