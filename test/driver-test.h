#ifndef DRIVER_TEST_H_
#define DRIVER_TEST_H_
#include "../src/driver.h"

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
};

#endif