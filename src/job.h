#ifndef JOB_H_
#define JOB_H_

#include "task.h"

struct Stats {
    std::vector<int> interpretation_vertices;
    std::vector<int> interpretation_edges;
    std::atomic<long> ec_count = 0;
};

struct Job {
    moodycamel::ConcurrentQueue<interpretation::Graph::Attributes> attributes_queue;
    bool check_subdomains = false;
    bool check_structural_delegations = false;
    moodycamel::ConcurrentQueue<unique_ptr<Task>> ec_queue;
    std::atomic<bool> finished_ec_generation = false;
    moodycamel::ConcurrentQueue<json> json_queue;
    vector<interpretation::Graph::NodeFunction> node_functions;
    vector<interpretation::Graph::PathFunction> path_functions;
    Stats stats;
    std::bitset<RRType::N> types_req;
    string user_input_domain;
};

#endif