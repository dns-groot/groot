#include "driver.h"
#include "ec-task.h"
#include "structural-task.h"
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

using namespace boost::accumulators;

void Driver::GenerateECsAndCheckProperties()
{
    if (current_job_.ec_queue.size_approx() != 0) {
        Logger->warn(fmt::format(
            "driver.cpp (GenerateECAndCheckProperties) - The EC queue is non-empty for {}",
            current_job_.user_input_domain));
    }
    current_job_.finished_ec_generation = false;

    // EC producer thread (internally calls other threads depending on the closest enclosers)
    std::thread label_graph_ec_generator = thread([&]() { label_graph_.GenerateECs(current_job_, context_); });

    std::thread EC_consumers[kECConsumerCount];
    std::atomic<int> done_consumers(0);

    vector<boost::any> structural_variadic;
    structural_variadic.push_back(&current_job_);
    structural_variadic.push_back(&label_graph_);

    vector<boost::any> ec_variadic;
    ec_variadic.push_back(&current_job_);

    // EC consumer threads which are also JSON producer threads.
    // After all the threads are finished the doneConsumers count would be ECConsumerCount+1
    for (int i = 0; i != kECConsumerCount; ++i) {
        EC_consumers[i] = thread([i, &structural_variadic, &ec_variadic, &done_consumers, this]() {
            bool itemsLeft;
            unique_ptr<Task> item;
            int id = i;
            do {
                itemsLeft = !current_job_.finished_ec_generation;
                while (current_job_.ec_queue.try_dequeue(item)) {
                    itemsLeft = true;
                    if (dynamic_cast<ECTask *>(item.get()) != nullptr) {
                        item->Process(context_, ec_variadic);
                    } else if (dynamic_cast<StructuralTask *>(item.get()) != nullptr) {
                        item->Process(context_, structural_variadic);
                    }
                }
            } while (itemsLeft || done_consumers.fetch_add(1, std::memory_order_acq_rel) + 1 == kECConsumerCount);
        });
    }
    // JSON consumer thread
    std::thread json_consumer = thread([this, &done_consumers]() {
        json item;
        bool itemsLeft;
        std::unordered_map<string, set<string>> aliases;
        do {
            itemsLeft = done_consumers.load(std::memory_order_acquire) <= kECConsumerCount;
            while (current_job_.json_queue.try_dequeue(item)) {
                itemsLeft = true;
                if (string(item["Property"]) == "All Aliases") {
                    if (aliases.find(string(item["Canonical Name"])) == aliases.end()) {
                        aliases[string(item["Canonical Name"])] = {};
                    }
                    aliases[string(item["Canonical Name"])].insert(string(item["Query"]));
                } else {
                    property_violations_.insert(item);
                }
            }
        } while (itemsLeft);
        if (aliases.size()) {
            json aliases_tmp;
            aliases_tmp["Property"] = "All Aliases";
            aliases_tmp["Canonical Name and their Aliases"] = {};
            for (auto &[k, v] : aliases) {
                json tmp;
                tmp["Canonical Name"] = k;
                tmp["Aliases"] = v;
                aliases_tmp["Canonical Name and their Aliases"].push_back(tmp);
            }
            property_violations_.insert(aliases_tmp);
        }
    });

    // Stats collector thread
    std::thread stats_collector = thread([this, &done_consumers]() {
        interpretation::Graph::Attributes item;
        bool itemsLeft;
        do {
            itemsLeft = done_consumers.load(std::memory_order_acquire) <= kECConsumerCount;
            while (current_job_.attributes_queue.try_dequeue(item)) {
                itemsLeft = true;
                current_job_.stats.interpretation_vertices.push_back(std::get<0>(item));
                current_job_.stats.interpretation_edges.push_back(std::get<1>(item));
            }
        } while (itemsLeft);
    });

    label_graph_ec_generator.join();
    current_job_.finished_ec_generation = true;
    for (int i = 0; i != kECConsumerCount; ++i) {
        EC_consumers[i].join();
    }
    json_consumer.join();
    stats_collector.join();
}

void Driver::GenerateAndOutputECs()
{
    current_job_.finished_ec_generation = false;

    std::ofstream ofs;
    ofs.open("ECs.txt", std::ofstream::out);

    // EC producer thread (internally calls other threads depending on the closest enclosers)
    std::thread label_graph_ec_generator = thread([&]() { label_graph_.GenerateECs(current_job_, context_); });

    std::thread ec_consumer = thread([this, &ofs]() {
        bool itemsLeft;
        unique_ptr<Task> item;
        do {
            itemsLeft = !current_job_.finished_ec_generation;
            while (current_job_.ec_queue.try_dequeue(item)) {
                itemsLeft = true;
                if (dynamic_cast<ECTask *>(item.get()) != nullptr) {
                    auto ec_task = dynamic_cast<ECTask *>(item.get());
                    ofs << ec_task->ec_.ToConcreteString() << "\n";
                }
            }
        } while (itemsLeft);
    });
    label_graph_ec_generator.join();
    current_job_.finished_ec_generation = true;
    ec_consumer.join();
    ofs.close();
}

long Driver::GetECCountForCurrentJob() const
{
    return current_job_.stats.ec_count;
}

void MetadataSanityCheck(const json &metadata, string directory)
{
    if (metadata.count("TopNameServers") != 1) {
        Logger->critical(fmt::format("driver.cpp (MetadataSanityCheck) - Metadata.json does not have the "
                                     "\"TopNameServers\" field. Check the example metadata.json format."));
        exit(EXIT_FAILURE);
    }
    std::unordered_set<string> top_nameservers;
    for (auto &server : metadata["TopNameServers"]) {
        string s = string(server);
        boost::to_lower(s);
        top_nameservers.insert(s);
    }
    if (metadata.count("ZoneFiles") != 1) {
        Logger->critical(fmt::format("driver.cpp (MetadataSanityCheck) - Metadata.json does not have the \"ZoneFiles\" "
                                     "field. Check the example metadata.json format."));
        exit(EXIT_FAILURE);
    }
    std::unordered_set<string> nameservers;
    for (auto &zone_json : metadata["ZoneFiles"]) {
        if (zone_json.count("FileName") != 1) {
            Logger->critical(fmt::format(
                "driver.cpp (MetadataSanityCheck) -\n{} \nThis item in the list of zone files "
                "doesn't have the \"FileName\" field. Check the example metadata.json format.",
                zone_json.dump(4)));
            exit(EXIT_FAILURE);
        }
        if (zone_json.count("NameServer") != 1) {
            Logger->critical(fmt::format(
                "driver.cpp (MetadataSanityCheck) -\n{} \nThis item in the list of zone files doesn't have "
                "the \"NameServer\" field. Check the example metadata.json format.",
                zone_json.dump(4)));
            exit(EXIT_FAILURE);
        }
        string ns = string(zone_json["NameServer"]);
        boost::to_lower(ns);
        nameservers.insert(ns);
        string file_name;
        zone_json["FileName"].get_to(file_name);
        auto zone_file_path = (boost::filesystem::path{directory} / boost::filesystem::path{file_name}).string();
        if (!boost::filesystem::exists(zone_file_path)) {
            Logger->critical(
                fmt::format("driver.cpp (MetadataSanityCheck) - ZoneFile {} doesn't exist", zone_file_path));
            exit(EXIT_FAILURE);
        }
    }
    bool atleast_one = false;
    for (auto &top : top_nameservers) {
        if (nameservers.find(top) != nameservers.end()) {
            atleast_one = true;
            break;
        }
    }
    if (!atleast_one) {
        Logger->critical(fmt::format(
            "driver.cpp (MetadataSanityCheck) - No TopNameServer is mentioned as nameserver for the zone files."));
        exit(EXIT_FAILURE);
    }
}

long Driver::SetContext(const json &metadata, string directory, bool lint)
{
    // TODO: Teardown if the context is set multiple times

    MetadataSanityCheck(metadata, directory);

    for (auto &server : metadata["TopNameServers"]) {
        string s = string(server);
        boost::to_lower(s);
        context_.top_nameservers.push_back(s);
    }
    long rr_count = 0;
    for (auto &zone_json : metadata["ZoneFiles"]) {
        string file_name;
        zone_json["FileName"].get_to(file_name);
        auto zone_file_path = (boost::filesystem::path{directory} / boost::filesystem::path{file_name}).string();
        string ns = string(zone_json["NameServer"]);
        boost::to_lower(ns);
        string origin = zone_json.count("Origin") ? string(zone_json["Origin"]) : "";
        boost::to_lower(origin);
        rr_count += ParseZoneFileAndExtendGraphs(zone_file_path, ns, origin, lint);
    }
    Logger->info(fmt::format("Total number of RRs parsed across all zone files: {}", rr_count));
    string types_info = "";
    for (auto &[k, v] : context_.type_to_rr_count) {
        types_info += k + ":" + to_string(v) + ", ";
    }
    Logger->info(fmt::format("RR Stats: {}", types_info));
    Logger->info(
        fmt::format("Label Graph: vertices = {}, edges = {}", num_vertices(label_graph_), num_edges(label_graph_)));
    if (lint) {
        for (auto &[id, z] : context_.zoneId_to_zone) {
            z.CheckGlueRecordsPresence(context_.zoneId_nameserver_map.at(id));
        }
    }
    return rr_count;
}

void Driver::SetJob(const json &user_job)
{
    string d = string(user_job["Domain"]);
    boost::to_lower(d);
    current_job_.stats.ec_count = 0;
    current_job_.stats.interpretation_edges.clear();
    current_job_.stats.interpretation_vertices.clear();
    current_job_.user_input_domain = d;
    current_job_.check_subdomains = user_job["SubDomain"];
    current_job_.path_functions.clear();
    current_job_.node_functions.clear();
    current_job_.types_req = {};

    for (auto &property : user_job["Properties"]) {
        string name = property["PropertyName"];
        std::bitset<RRType::N> propertyTypes;
        if (property.find("Types") != property.end()) {
            for (auto typ : property["Types"]) {
                current_job_.types_req.set(TypeUtils::StringToType(typ));
                propertyTypes.set(TypeUtils::StringToType(typ));
            }
        } else {
            current_job_.types_req.set(RRType::NS);
        }
        if (name == "ResponseConsistency") {
            auto la = [propertyTypes](
                          const interpretation::Graph &graph,
                          const vector<interpretation::Graph::VertexDescriptor> &end,
                          moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::CheckSameResponseReturned(graph, end, json_queue, propertyTypes);
            };
            current_job_.node_functions.push_back(la);
        } else if (name == "ResponseReturned") {
            auto la = [propertyTypes](
                          const interpretation::Graph &graph,
                          const vector<interpretation::Graph::VertexDescriptor> &end,
                          moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::CheckResponseReturned(graph, end, json_queue, propertyTypes);
            };
            current_job_.node_functions.push_back(la);
        } else if (name == "ResponseValue") {
            std::set<string> values;
            for (string v : property["Value"]) {
                values.insert(v);
            }
            if (values.size()) {
                auto la = [types = std::move(propertyTypes), v = std::move(values)](
                              const interpretation::Graph &graph,
                              const vector<interpretation::Graph::VertexDescriptor> &end,
                              moodycamel::ConcurrentQueue<json> &json_queue) {
                    interpretation::Graph::Properties::CheckResponseValue(graph, end, json_queue, types, v);
                };
                current_job_.node_functions.push_back(la);
            } else {
                Logger->error(fmt::format(
                    "driver.cpp (SetJob) - Skipping ResponseValue property check for {} as the "
                    "Values is an empty list.",
                    string(user_job["Domain"])));
            }
        } else if (name == "Hops") {
            auto l = [num_hops = property["Value"]](
                         const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                         moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::NumberOfHops(graph, p, json_queue, num_hops);
            };
            current_job_.path_functions.push_back(l);
        } else if (name == "Rewrites") {
            auto l = [num_rewrites = property["Value"]](
                         const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                         moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::NumberOfRewrites(graph, p, json_queue, num_rewrites);
            };
            current_job_.path_functions.push_back(l);
        } else if (name == "DelegationConsistency") {
            auto l = [&](const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                         moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::CheckDelegationConsistency(graph, p, json_queue);
            };
            current_job_.path_functions.push_back(l);
        } else if (name == "InfiniteDNameRec") {
            auto l = [&](const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                         moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::InfiniteDName(graph, p, json_queue);
            };
            current_job_.path_functions.push_back(l);
        } else if (name == "LameDelegation") {
            auto l = [&](const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                         moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::CheckLameDelegation(graph, p, json_queue);
            };
            current_job_.path_functions.push_back(l);
        } else if (name == "QueryRewrite") {
            vector<vector<NodeLabel>> allowed_domains;
            for (string v : property["Value"]) {
                boost::to_lower(v);
                allowed_domains.push_back(LabelUtils::StringToLabels(v));
            }
            if (allowed_domains.size() > 0) {
                auto l = [d = std::move(allowed_domains)](
                             const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                             moodycamel::ConcurrentQueue<json> &json_queue) {
                    interpretation::Graph::Properties::QueryRewrite(graph, p, json_queue, d);
                };
                current_job_.path_functions.push_back(l);
            } else {
                Logger->error(fmt::format(
                    "driver.cpp (SetJob) - Skipping QueryRewrite property check for {} as the Values is an empty list.",
                    string(user_job["Domain"])));
            }
        } else if (name == "NameserverContact") {
            vector<vector<NodeLabel>> allowed_domains;
            for (string v : property["Value"]) {
                boost::to_lower(v);
                allowed_domains.push_back(LabelUtils::StringToLabels(v));
            }
            if (allowed_domains.size() > 0) {
                auto l = [d = std::move(allowed_domains)](
                             const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                             moodycamel::ConcurrentQueue<json> &json_queue) {
                    interpretation::Graph::Properties::NameServerContact(graph, p, json_queue, d);
                };
                current_job_.path_functions.push_back(l);
            } else {
                Logger->error(fmt::format(
                    "driver.cpp (SetJob) - Skipping NameServerContact property check for {} as "
                    "the Values is an empty list.",
                    string(user_job["Domain"])));
            }
        } else if (name == "RewriteBlackholing") {
            current_job_.path_functions.push_back(interpretation::Graph::Properties::RewriteBlackholing);
        } else if (name == "AllAliases") {
            vector<vector<NodeLabel>> canonical_names;
            for (string v : property["Value"]) {
                boost::to_lower(v);
                canonical_names.push_back(LabelUtils::StringToLabels(v));
            }
            if (canonical_names.size() > 0) {
                auto l = [d = std::move(canonical_names)](
                             const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                             moodycamel::ConcurrentQueue<json> &json_queue) {
                    interpretation::Graph::Properties::AllAliases(graph, p, json_queue, d);
                };
                current_job_.path_functions.push_back(l);
            } else {
                Logger->error(fmt::format(
                    "driver.cpp (SetJob) - Skipping AlliAliases property check for {} as the Values is an empty list.",
                    string(user_job["Domain"])));
            }
        } else if (name == "ZeroTTL") {
            auto la = [propertyTypes](
                          const interpretation::Graph &graph,
                          const vector<interpretation::Graph::VertexDescriptor> &end,
                          moodycamel::ConcurrentQueue<json> &json_queue) {
                interpretation::Graph::Properties::ZeroTTL(graph, end, json_queue, propertyTypes);
            };
            current_job_.node_functions.push_back(la);
        } else if (name == "StructuralDelegationConsistency") {
            current_job_.check_structural_delegations = true;
        } else if (name == "DNAMESubstitutionCheck") {
            current_job_.path_functions.push_back(interpretation::Graph::Properties::DNAMESubstitutionExceedesLength);
        }
    }
}

void Driver::SetJob(string &domain_name)
{
    boost::to_lower(domain_name);
    current_job_.stats.ec_count = 0;
    current_job_.stats.interpretation_edges.clear();
    current_job_.stats.interpretation_vertices.clear();
    current_job_.user_input_domain = domain_name;
    current_job_.check_subdomains = true;
    current_job_.path_functions.clear();
    current_job_.node_functions.clear();
    current_job_.types_req = {};

    current_job_.types_req.set(RRType::NS);

    auto hops = [num_hops = 2](
                    const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                    moodycamel::ConcurrentQueue<json> &json_queue) {
        interpretation::Graph::Properties::NumberOfHops(graph, p, json_queue, num_hops);
    };
    current_job_.path_functions.push_back(hops);

    auto rewrites = [num_rewrites = 1](
                        const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                        moodycamel::ConcurrentQueue<json> &json_queue) {
        interpretation::Graph::Properties::NumberOfRewrites(graph, p, json_queue, num_rewrites);
    };
    current_job_.path_functions.push_back(rewrites);

    auto delegation_consistency = [&](const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                                      moodycamel::ConcurrentQueue<json> &json_queue) {
        interpretation::Graph::Properties::CheckDelegationConsistency(graph, p, json_queue);
    };
    current_job_.path_functions.push_back(delegation_consistency);

    auto lame_delegation = [&](const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                               moodycamel::ConcurrentQueue<json> &json_queue) {
        interpretation::Graph::Properties::CheckLameDelegation(graph, p, json_queue);
    };
    current_job_.path_functions.push_back(lame_delegation);

    vector<vector<NodeLabel>> allowed_domains;
    allowed_domains.push_back(LabelUtils::StringToLabels(domain_name));
    auto query_rewrite = [d = std::move(allowed_domains)](
                             const interpretation::Graph &graph, const interpretation::Graph::Path &p,
                             moodycamel::ConcurrentQueue<json> &json_queue) {
        interpretation::Graph::Properties::QueryRewrite(graph, p, json_queue, d);
    };
    current_job_.path_functions.push_back(query_rewrite);

    current_job_.path_functions.push_back(interpretation::Graph::Properties::RewriteBlackholing);
    // current_job_.check_structural_delegations = true;
}

void Driver::WriteStatsForAJob()
{

    Logger->info(fmt::format("Number of ECs: {}", current_job_.stats.ec_count));
    /*auto [min, max] = std::minmax_element(
        begin(current_job_.stats.interpretation_vertices), end(current_job_.stats.interpretation_vertices));*/
    accumulator_set<double, features<tag::mean, tag::median, tag::min, tag::max>> acc;
    for_each(
        begin(current_job_.stats.interpretation_vertices), end(current_job_.stats.interpretation_vertices), ref(acc));
    Logger->info(fmt::format(
        "Interpretation Graph Vertices: Max={}, Min={}, Mean={}, Median={}", boost::accumulators::max(acc),
        boost::accumulators::min(acc), mean(acc), median(acc)));
    acc = {};
    for_each(begin(current_job_.stats.interpretation_edges), end(current_job_.stats.interpretation_edges), ref(acc));
    Logger->info(fmt::format(
        "Interpretation Graph Edges: Max={}, Min={}, Mean={}, Median={}", boost::accumulators::max(acc),
        boost::accumulators::min(acc), mean(acc), median(acc)));
}

void Driver::WriteViolationsToFile(string output_file) const
{
    // DumpNameServerZoneMap();
    Logger->info(fmt::format("Total number of violations: {}", property_violations_.size()));
    std::ofstream ofs;
    ofs.open(output_file, std::ofstream::out);
    ofs << "[\n";
    int c = 0;
    for (const json &j : property_violations_) {
        if (c != 0)
            ofs << ",\n";
        ofs << j.dump(4);
        c++;
    }
    ofs << "\n]";
    ofs.close();
    Logger->debug(fmt::format("driver.cpp (WriteViolationsToFile) - Output written to {}", output_file));
}

/* void Driver::DumpNameServerZoneMap() const
{
    boost::unordered_map<int, string> zoneId_to_zone_name;

    for (auto const &[key, val] : context_.zoneId_to_zone) {
        zoneId_to_zone_name.insert({key, LabelUtils::LabelsToString(val.get_origin())});
    }
    json j = {};
    for (auto const &[ns, zoneIds] : context_.nameserver_zoneIds_map) {
        j[ns] = {};
        for (auto const &i : zoneIds) {
            j[ns].push_back(zoneId_to_zone_name.at(i));
        }
    }
    std::ofstream ofs;
    ofs.open("Graphs/nameserver_map.json", std::ofstream::out);
    ofs << j.dump(4);
    ofs.close();
} */
