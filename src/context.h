#pragma once

#include "zone-graph.h"

struct Context {
	boost::unordered_map<string, std::vector<int>> nameserver_zoneIds_map_;
	std::vector<string> top_nameservers_;
	boost::unordered_map<int, zone::Graph> zoneId_to_zone_;
	int zoneId_counter_ = 0;
};