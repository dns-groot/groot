#include <boost/algorithm/string.hpp> 
#include <cstdlib> 
#include <iostream> 
#include <string> 
#include <vector>
#include <fstream>
#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/topological_sort.hpp"
#include <boost/graph/graphviz.hpp>
#include "zone.h"
#include "graph.h"
//#include "interpreter.h"
#include "interpreter.h"

using namespace std;


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
		else if (type == cname) {
			out << "[color=green]";
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

inline char const* type_to_string(edge_type c) {
	switch (c) {
	case edge_type::normal:   return "black";
	case edge_type::cname:   return "green";
	case edge_type::dname:   return "red";
	}
	return ""; // not known
}

template<class T>
void serialize(T& data, string fileName) {
	std::ofstream outputfile{fileName};
	boost::archive::text_oarchive oa{ outputfile };
	oa << data;
}

template<class T>
void deserialize(T& data, string fileName) {
	std::ifstream file1{fileName};
	boost::archive::text_iarchive ia(file1);
	ia >> data;
}

void comTesting(string& file) {
	vector<ResourceRecord> records;
	std::ifstream infile(file);
	std::string line;
	while (std::getline(infile, line))
	{
		ResourceRecord RR(line,"A", rr_class::CLASS_IN, 86400, "10.12.14.16");
		records.push_back(RR);
	}

	labelGraph g;
	//Add root node
	vertex_t root = boost::add_vertex(g);
	g[root].name = ".";
	label_graph_builder(records, g, root);

	zone z;
	zone_graph_builder(records, z);
	serialize(z, "com_Zone.txt");

	vector<EC> allQueries;
	ECgen(g, root, allQueries);
	serialize(allQueries, "com_EC.txt");

}

int main2(int argc, char* argv[])
{
	// get the zone file input as a string.
	string file(argv[1]);

	//comTesting(file);
	auto records = parse_zone_file(file);

	/*for (auto& record : records)
	{
		cout << record;
	}*/

	labelGraph g;
	//Add root node
	vertex_t root = boost::add_vertex(g);
	g[root].name = ".";
	label_graph_builder(records, g, root);
	
	zone z;
	zone_graph_builder(records, z);
	
	serialize(z,"zoneGraph.txt");

	
	std::ofstream dotfile("graph.dot");
	write_graphviz(dotfile, g, boost::make_label_writer(boost::get(&labelVertex::name, g)), make_edge_writer(boost::get(&labelEdge::type, g)));
	
	//Using dynamic properties
	boost::dynamic_properties dp;
	dp.property("node_id", get(&labelVertex::name, g));
	dp.property("label", get(&labelVertex::name, g));
	dp.property("color", boost::make_transform_value_property_map(&type_to_string, get(&labelEdge::type, g)));
	write_graphviz_dp(std::cout, g, dp);
	
	vector<EC> allQueries;
	ECgen(g, root, allQueries);
	
	serialize(allQueries,"EC.txt");
	//boost::optional<vector<ResourceRecord>> answer = queryLookUp(z, allQueries[1]);
	return 0;
}
std::map<string, std::vector<zone>> nameServer_Zone_Map;
int main(int argc, char* argv[]) {

	string directory(argv[1]);
	string net_sy = directory + "net.sy.txt";
	auto records = parse_zone_file(net_sy);

	labelGraph g;
	vertex_t root = boost::add_vertex(g);
	g[root].name = ".";
	label_graph_builder(records, g, root);

	zone net;
	zone_graph_builder(records, net);
	std::vector<zone> zones;
	zones.push_back(net);
	
	nameServer_Zone_Map.insert(std::pair<string, vector<zone>>( "ns1.tld.sy.",std::move(zones)));

	string mtn_net_sy = directory + "mtn.net.sy.txt";
	records = parse_zone_file(mtn_net_sy);
	label_graph_builder(records, g, root);
	
	zone mtn_net;
	zone_graph_builder(records, mtn_net);

	zones.clear();
	zones.push_back(mtn_net);
	nameServer_Zone_Map.insert(std::pair<string, vector<zone>>("ns1.mtn.net.sy.", std::move(zones)));

	vector<EC> allQueries;
	ECgen(g, root, allQueries);

	std::ofstream dotfile("LabelGraph.dot");
	write_graphviz(dotfile, g, boost::make_label_writer(boost::get(&labelVertex::name, g)), make_edge_writer(boost::get(&labelEdge::type, g)));

	//std::ofstream outfile("IntGraph.dot");
	for (auto& query : allQueries) {
		intGraph g;
		build_interpreter_graph("ns1.tld.sy.", query, g);
		//write_graphviz(outfile, g, boost::make_label_writer(boost::get(&intVertex::ns, g)));
	}
	return 0;
}