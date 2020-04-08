#ifndef JOB_H_
#define JOB_H_

#include "interpretation-properties.h"

struct Job {
	bool check_subdomains = false;
	std::atomic<long> ec_count = 0;
	moodycamel::ConcurrentQueue<EC> ec_queue;
	std::atomic<bool> finished_ec_generation = false;
	moodycamel::ConcurrentQueue<json> json_queue;
	vector<interpretation::Graph::NodeFunction> node_functions;
	vector<interpretation::Graph::PathFunction> path_functions;
	string user_input_domain;
	std::bitset<RRType::N> types_req;
};

#endif