#include <boost/regex.hpp>
#include "csvparser.h"


// used to split the file in lines
const boost::regex linesregx("\\r\\n|\\n\\r|\\n|\\r");

// used to split each line to tokens, assuming ',' as column separator
const boost::regex fieldsregx(",(?=(?:[^\"]*\"[^\"]*\")*(?![^\"]*\"))");



inline string ReadFromFile(char const* infile)
{
	ifstream instream(infile);
	if (!instream.is_open()) {
		cerr << "Couldn't open file: " << infile << endl;
		exit(-1);
	}
	instream.unsetf(ios::skipws);
	return string(std::istreambuf_iterator<char>(instream.rdbuf()),
		istreambuf_iterator<char>());
}

inline bool FileExists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

tuple<string, string> BuildSOALabelGraph(Row resourceRecord, LabelGraph& g, const VertexDescriptor root, string outputDirectory) {

	if (!FileExists((boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ resourceRecord[0] + ".txt" }).string())) {
		int index = 0;
		auto l = GetLabels(resourceRecord[0]);
		VertexDescriptor closetEncloser = GetClosestEncloser(g, root, l, index);
		VertexDescriptor mainNode = AddNodes(g, closetEncloser, l, index);
		g[mainNode].rrTypesAvailable.set(RRType::SOA);
		ofstream myfile;
		myfile.open((boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ resourceRecord[0] + ".txt" }).string(), ios::out | ios::app);
		string domain = resourceRecord[0];
		resourceRecord.erase(resourceRecord.begin());
		resourceRecord.erase(resourceRecord.begin());
		string r = domain + "\t86400\tIN\tSOA\t" + boost::algorithm::join(resourceRecord, "\t") + "\n";;
		myfile << r;
		myfile.close();
		return std::make_tuple(domain + ".txt", resourceRecord[0]);
	}
	else {
		return {};
	}

}

//void SOA_CSV_Parser(string file, LabelGraph& g, const VertexDescriptor root, string outputDirectory)
//{
//    string data(ReadFromFile(file.c_str()));
//    std::vector<Row> result;
//    // iterator splits data to lines
//    boost::sregex_token_iterator li(data.begin(), data.end(), linesregx, -1);
//    boost::sregex_token_iterator end;
//    int i = 0;
//    while (li != end) {
//        std::string line = li->str();
//        ++li;
//
//        // Split line to tokens
//        boost::sregex_token_iterator ti(line.begin(), line.end(), fieldsregx, -1);
//        boost::sregex_token_iterator end2;
//
//        std::vector<std::string> row;
//        while (ti != end2) {
//            std::string token = ti->str();
//            ++ti;
//            row.push_back(token);
//        }
//        if (line.back() == ',') {
//            // last character was a separator
//            row.push_back("");
//        }
//        BuildSOALabelGraph(row, g, root, outputDirectory);
//        i++;
//        if (i > 10) exit(EXIT_SUCCESS);
//    }
//}

void SOA_CSV_Parser(string file, LabelGraph& g, const VertexDescriptor root, string outputDirectory)
{
	std::ifstream infile(file);
	std::string line;
	int i = 0;
	json j;
	j["ZoneFiles"] = {};
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
		if (i != 0) {
			tuple<string, string> zoneNSpair = BuildSOALabelGraph(row, g, root, outputDirectory);
			if (std::get<0>(zoneNSpair).size()) {
				json tmp;
				tmp["FileName"] = std::get<0>(zoneNSpair);
				tmp["NameServer"] = std::get<1>(zoneNSpair);
				j["ZoneFiles"].push_back(tmp);
			}
		}
		i++;
		//if (i > 15500) break;
	}
	cout << "Total Lines in SOA: " << i << endl;
	std::ofstream ofs;
	ofs.open((boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ "metadata.json" }).string(), std::ofstream::out | std::ofstream::app);
	ofs << j.dump(4);
	ofs << "\n";
	ofs.close();
	
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\dname.csv", "DNAME", g, root, outputDirectory);
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\cname.csv", "CNAME", g, root, outputDirectory);
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\txt.csv", "TXT", g, root, outputDirectory);
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\mx.csv", "MX", g, root, outputDirectory);
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\a.csv", "A", g, root, outputDirectory);
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\aaaa.csv", "AAAA", g, root, outputDirectory);
	Generate_Zone_Files("C:\\Users\\Administrator\\Desktop\\DNS\\DNSCensus2013\\records\\ns.csv", "NS", g, root, outputDirectory);
}

tuple<string, string> SOALabelGraphGetClosestEnclosers(const LabelGraph& g, VertexDescriptor root, vector<Label> labels, int& index, string path, string child, string parent) {
	VertexDescriptor closestEncloser = root;
	if (labels.size() == index) {
		return make_tuple(parent, child);
	}
	if (g[closestEncloser].name.get().length())	path = g[closestEncloser].name.get() + "." + path;
	if (out_degree(closestEncloser, g) > kHashMapThreshold) {
		if (gDomainChildLabelMap.find(closestEncloser) == gDomainChildLabelMap.end()) {
			gDomainChildLabelMap[closestEncloser] = ConstructLabelMap(g, closestEncloser);
		}
		LabelMap& m = gDomainChildLabelMap.find(closestEncloser)->second;
		auto it = m.find(labels[index]);
		if (it != m.end()) {
			closestEncloser = it->second;
			if (g[closestEncloser].rrTypesAvailable[RRType::SOA]) {
				parent = child;
				child = g[closestEncloser].name.get() + "." + path;
			}
			index++;
			return SOALabelGraphGetClosestEnclosers(g, closestEncloser, labels, index, path, child, parent);
		}
	}
	else {
		for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closestEncloser, g))) {
			if (g[edge].type == normal) {
				if (g[edge.m_target].name == labels[index]) {
					if (g[edge.m_target].rrTypesAvailable[RRType::SOA]) {
						parent = child;
						child = g[edge.m_target].name.get() + "." +path;
					}
					closestEncloser = edge.m_target;
					index++;
					return SOALabelGraphGetClosestEnclosers(g, closestEncloser, labels, index, path, child, parent);
				}
			}
		}
	}
	return  make_tuple(parent, child);
}

void OutputToFile(Row resourceRecord, string outputDirectory, string fileName, string type) {
	
	if (FileExists((boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ fileName + ".txt" }).string())) {
		ofstream myfile;
		myfile.open((boost::filesystem::path{ outputDirectory } / boost::filesystem::path{ fileName + ".txt" }).string(), ios::out | ios::app);
		string domain = resourceRecord[0];
		resourceRecord.erase(resourceRecord.begin());
		resourceRecord.erase(resourceRecord.begin());
		string r = domain + "\tIN\t" + type + "\t" + boost::algorithm::join(resourceRecord, "\t") + "\n";
		myfile << r;
		myfile.close();
	}	
}

void Generate_Zone_Files(string file, string type, LabelGraph& g, VertexDescriptor root, string outputDirectory) {
	std::ifstream infile(file);
	std::string line;
	int i = 0;
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
		if (i != 0) {
			int index = 0;
			tuple<string, string> pc = SOALabelGraphGetClosestEnclosers(g, root, GetLabels(row[0]), index, "", ".", "");
			if (type != "NS") {
				OutputToFile(row, outputDirectory, std::get<1>(pc), type);
			}
			else {
				OutputToFile(row, outputDirectory, std::get<1>(pc), type);
				OutputToFile(row, outputDirectory, std::get<0>(pc), type);
			}
			cout << std::get<0>(pc);
		}
		i++;
		//if (i > 10) break;
	}

}