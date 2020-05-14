#ifndef DRIVER_TEST_H_
#define DRIVER_TEST_H_
#include "../src/driver.h"
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

class DriverTest {

public:
	int GetNumberofLabelGraphVertices(Driver& d) {
		return static_cast<int>(num_vertices(d.label_graph_));
	}

	int GetNumberofLabelGraphEdges(Driver& d) {
        return static_cast<int>(num_edges(d.label_graph_));
	}

	int GetNumberofResourceRecordsParsed(Driver& d, string file, string nameserver) {
		return d.ParseZoneFileAndExtendGraphs(file, nameserver, "");
	}

	int GetNumberofViolations(Driver& d) {
        return static_cast<int>(d.property_violations_.size());
	}

	boost::unordered_map<string, long> GetTypeToCountMap(Driver& d) {
		return d.context_.type_to_rr_count;
	}

	const zone::Graph& GetLatestZone(Driver& d) {
		return d.context_.zoneId_to_zone.at(d.context_.zoneId_counter_);
	}

	interpretation::Graph CreateAnInterpretationGraph(Driver& d,const EC ec) {
		return interpretation::Graph(ec, d.context_);
	}

	boost::filesystem::path GetTestDirectoryPath() {
		boost::filesystem::path current_directory(boost::filesystem::current_path());
		boost::filesystem::path accumulated_path{};
		for (auto& part : current_directory) {
			accumulated_path /= part;
			if (part == "groot") {
				boost::filesystem::path tmp = accumulated_path / "test";
				if (is_directory(tmp)) {
					return tmp;
				}
			}
		}
		throw std::runtime_error("Could not find the test directory");
	}

	boost::filesystem::path GetDemoDirectoryPath() {
		boost::filesystem::path current_directory(boost::filesystem::current_path());
		boost::filesystem::path accumulated_path{};
		for (auto& part : current_directory) {
			accumulated_path /= part;
			if (part == "groot") {
				boost::filesystem::path tmp = accumulated_path / "demo";
				if (is_directory(tmp)) {
					return tmp;
				}
			}
		}
		throw std::runtime_error("Could not find the demo directory");
	}
};

#endif