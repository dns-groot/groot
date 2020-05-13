#ifndef JOB_H_
#define JOB_H_

#include "task.h"

struct Job {
	bool check_subdomains = false;
	bool check_structural_delegations = false;
	std::atomic<long> ec_count = 0;
	moodycamel::ConcurrentQueue<unique_ptr<Task>> ec_queue;
	std::atomic<bool> finished_ec_generation = false;
	moodycamel::ConcurrentQueue<json> json_queue;
	vector<interpretation::Graph::NodeFunction> node_functions;
	vector<interpretation::Graph::PathFunction> path_functions;
	string user_input_domain;
	std::bitset<RRType::N> types_req;
};

#endif