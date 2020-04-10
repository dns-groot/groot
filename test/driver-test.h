#ifndef DRIVER_TEST_H_
#define DRIVER_TEST_H_
#include "../src/driver.h"
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

class DriverTest {

public:
	int GetNumberofLabelGraphVertices(Driver& d) {
		return num_vertices(d.label_graph_);
	}

	int GetNumberofLabelGraphEdges(Driver& d) {
		return num_edges(d.label_graph_);
	}

	int GetNumberofResourceRecordsParsed(Driver& d, string file, string nameserver) {
		return d.ParseZoneFileAndExtendGraphs(file, nameserver);
	}

	const zone::Graph& GetLatestZone(Driver& d) {
		return d.context_.zoneId_to_zone_.at(d.context_.zoneId_counter_);
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
};

#endif