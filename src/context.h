#ifndef CONTEXT_H
#define CONTEXT_H

#include "zone-graph.h"

struct NSdummy{};

using Nameserver = boost::flyweight<std::string, boost::flyweights::tag<NSdummy>, boost::flyweights::no_tracking>;

struct Context {
	boost::unordered_map<string, std::vector<int>> nameserver_zoneIds_map;
	boost::unordered_map<int, Nameserver> zoneId_nameserver_map;
	std::vector<string> top_nameservers;
	boost::unordered_map<int, zone::Graph> zoneId_to_zone;
	boost::unordered_map<string, long> type_to_rr_count;
	int zoneId_counter_ = 0;
};

#endif