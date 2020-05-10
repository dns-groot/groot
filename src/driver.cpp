#include "driver.h"
#include <boost/filesystem.hpp>

void Driver::GenerateECsAndCheckProperties()
{
	if (current_job_.ec_queue.size_approx() != 0) {
		Logger->warn(fmt::format("driver.cpp (GenerateECAndCheckProperties) - The EC queue is non-empty for {}", current_job_.user_input_domain));
	}
	current_job_.finished_ec_generation = false;

	//EC producer thread (internally calls other threads depending on the closest enclosers)
	std::thread label_graph_ec_generator = thread([&]() {
		label_graph_.GenerateECs(current_job_, context_);
		});

	std::thread EC_consumers[kECConsumerCount];
	std::atomic<int> done_consumers(0);

	//EC consumer threads which are also JSON producer threads.
	//After all the threads are finished the doneConsumers count would be ECConsumerCount+1
	for (int i = 0; i != kECConsumerCount; ++i) {
		EC_consumers[i] = thread([i, &done_consumers, this]() {
			EC item;
			bool itemsLeft;
			int id = i;
			do {
				itemsLeft = !current_job_.finished_ec_generation;
				while (current_job_.ec_queue.try_dequeue(item)) {
					itemsLeft = true;
					current_job_.ec_count++;
					interpretation::Graph interpretation_graph_for_ec(item, context_);
					//interpretation_graph_for_ec.CheckForLoops(json_queue_);
					//interpretation_graph_for_ec.GenerateDotFile("InterpretationGraph.dot");
					interpretation_graph_for_ec.CheckPropertiesOnEC(current_job_.path_functions, current_job_.node_functions, current_job_.json_queue);
				}
			} while (itemsLeft || done_consumers.fetch_add(1, std::memory_order_acq_rel) + 1 == kECConsumerCount);
			});
	}
	//JSON consumer thread
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
					aliases[string(item["Canonical Name"])].insert(string(item["Equivalence Class"]));
				}
				else {
					property_violations_.insert(item);
				}
			}
		} while (itemsLeft);
		if (aliases.size()) {
			json aliases_tmp;
			aliases_tmp["Property"] = "All Aliases";
			aliases_tmp["Canonical Name and their Aliases"] = {};
			for (auto& [k, v] : aliases) {
				json tmp;
				tmp["Canonical Name"] = k;
				tmp["Aliases"] = v;
				aliases_tmp["Canonical Name and their Aliases"].push_back(tmp);
			}
			property_violations_.insert(aliases_tmp);
		}		
		});
	label_graph_ec_generator.join();
	current_job_.finished_ec_generation = true;
	for (int i = 0; i != kECConsumerCount; ++i) {
		EC_consumers[i].join();
	}
	json_consumer.join();
}

long Driver::GetECCountForCurrentJob() const
{
	return current_job_.ec_count;
}

long Driver::SetContext(const json& metadata, string directory)
{
	//TODO: Teardown if the context is set multiple times
	for (auto& server : metadata["TopNameServers"]) {
		context_.top_nameservers.push_back(server);
	}
	long rr_count = 0;
	for (auto& zone_json : metadata["ZoneFiles"]) {
		string file_name;
		zone_json["FileName"].get_to(file_name);
		auto zone_file_path = (boost::filesystem::path{ directory } / boost::filesystem::path{ file_name }).string();
		rr_count += ParseZoneFileAndExtendGraphs(zone_file_path, zone_json["NameServer"], zone_json.count("Origin")?string(zone_json["Origin"]): "");
	}
	Logger->info(fmt::format("Total number of RRs parsed across all zone files: {}", rr_count));
	string types_info = "";
	for (auto& [k, v] : context_.type_to_rr_count) {
		types_info += k + ":" +  to_string(v) + ", ";
	}
	Logger->info(fmt::format("RR Stats: {}", types_info));
	return rr_count;
}

void Driver::SetJob(const json& user_job)
{
	current_job_.ec_count = 0;
	current_job_.user_input_domain = string(user_job["Domain"]);
	current_job_.check_subdomains = user_job["SubDomain"];
	current_job_.path_functions.clear();
	current_job_.node_functions.clear();
	current_job_.types_req = {};

	for (auto& property : user_job["Properties"]) {
		string name = property["PropertyName"];
		std::bitset<RRType::N> propertyTypes;
		if (property.find("Types") != property.end()) {
			for (auto typ : property["Types"]) {
				current_job_.types_req.set(TypeUtils::StringToType(typ));
				propertyTypes.set(TypeUtils::StringToType(typ));
			}
		}
		else {
			current_job_.types_req.set(RRType::NS);
		}
		if (name == "ResponseConsistency") {
			auto la = [propertyTypes](const interpretation::Graph& graph, const vector<interpretation::Graph::VertexDescriptor>& end, moodycamel::ConcurrentQueue<json>& json_queue) { interpretation::Graph::Properties::CheckSameResponseReturned(graph, end, json_queue, propertyTypes); };
			current_job_.node_functions.push_back(la);
		}
		else if (name == "ResponseReturned") {
			auto la = [propertyTypes](const interpretation::Graph& graph, const vector<interpretation::Graph::VertexDescriptor>& end, moodycamel::ConcurrentQueue<json>& json_queue) { interpretation::Graph::Properties::CheckResponseReturned(graph, end, json_queue, propertyTypes); };
			current_job_.node_functions.push_back(la);
		}
		else if (name == "ResponseValue") {
			std::set<string> values;
			for (string v : property["Value"]) {
				values.insert(v);
			}
			if (values.size()) {
				auto la = [types = std::move(propertyTypes), v = std::move(values)](const interpretation::Graph& graph, const vector<interpretation::Graph::VertexDescriptor>& end, moodycamel::ConcurrentQueue<json>& json_queue){ interpretation::Graph::Properties::CheckResponseValue(graph, end, json_queue, types, v); };
				current_job_.node_functions.push_back(la);
			}
			else {
				Logger->error(fmt::format("driver.cpp (SetJob) - Skipping ResponseValue property check for {} as the Values is an empty list.", string(user_job["Domain"])));
			}
		}
		else if (name == "Hops") {
			auto l = [num_hops = property["Value"]](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::NumberOfHops(graph, p, json_queue, num_hops); };
			current_job_.path_functions.push_back(l);
		}
		else if (name == "Rewrites") {
			auto l = [num_rewrites = property["Value"]](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::NumberOfRewrites(graph, p, json_queue, num_rewrites); };
			current_job_.path_functions.push_back(l);
		}
		else if (name == "DelegationConsistency") {
			auto l = [&](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::CheckDelegationConsistency(graph, p, json_queue); };
			current_job_.path_functions.push_back(l);
		}
		else if (name == "LameDelegation") {
			auto l = [&](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::CheckLameDelegation(graph, p, json_queue); };
			current_job_.path_functions.push_back(l);
		}
		else if (name == "QueryRewrite") {
			vector<vector<NodeLabel>> allowed_domains;
			for (string v : property["Value"]) {
				allowed_domains.push_back(LabelUtils::StringToLabels(v));
			}
			if (allowed_domains.size() > 0) {
				auto l = [d = std::move(allowed_domains)](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::QueryRewrite(graph, p, json_queue, d); };
				current_job_.path_functions.push_back(l);
			}
			else {
				Logger->error(fmt::format("driver.cpp (SetJob) - Skipping QueryRewrite property check for {} as the Values is an empty list.", string(user_job["Domain"])));
			}
		}
		else if (name == "NameserverContact") {
			vector<vector<NodeLabel>> allowed_domains;
			for (string v : property["Value"]) {
				allowed_domains.push_back(LabelUtils::StringToLabels(v));
			}
			if (allowed_domains.size() > 0) {
				auto l = [d = std::move(allowed_domains)](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::NameServerContact(graph, p, json_queue, d); };
				current_job_.path_functions.push_back(l);
			}
			else {
				Logger->error(fmt::format("driver.cpp (SetJob) - Skipping NameServerContact property check for {} as the Values is an empty list.", string(user_job["Domain"])));
			}
		}
		else if (name == "RewriteBlackholing") {
			current_job_.path_functions.push_back(interpretation::Graph::Properties::RewriteBlackholing);
		}
		else if (name == "AllAliases") {
			vector<vector<NodeLabel>> canonical_names;
			for (string v : property["Value"]) {
				canonical_names.push_back(LabelUtils::StringToLabels(v));
			}
			if (canonical_names.size() > 0) {
				auto l = [d = std::move(canonical_names)](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::AllAliases(graph, p, json_queue, d); };
				current_job_.path_functions.push_back(l);
			}
			else {
				Logger->error(fmt::format("driver.cpp (SetJob) - Skipping AlliAliases property check for {} as the Values is an empty list.", string(user_job["Domain"])));
			}
		}
		else if (name == "StructuralDelegationConsistency") {
			current_job_.check_structural_delegations = true;
		}
	}
}

void Driver::SetJob(const string& second_level_tld) {
	current_job_.ec_count = 0;
	current_job_.user_input_domain = second_level_tld;
	current_job_.check_subdomains = true;
	current_job_.path_functions.clear();
	current_job_.node_functions.clear();
	current_job_.types_req = {};

	current_job_.types_req.set(RRType::NS);

	auto hops = [num_hops = 2](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::NumberOfHops(graph, p, json_queue, num_hops); };
	current_job_.path_functions.push_back(hops);

	auto rewrites = [num_rewrites = 1](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::NumberOfRewrites(graph, p, json_queue, num_rewrites); };
	current_job_.path_functions.push_back(rewrites);

	auto delegation_consistency = [&](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::CheckDelegationConsistency(graph, p, json_queue); };
	current_job_.path_functions.push_back(delegation_consistency);

	auto lame_delegation = [&](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::CheckLameDelegation(graph, p, json_queue); };
	current_job_.path_functions.push_back(lame_delegation);

	vector<vector<NodeLabel>> allowed_domains;
	allowed_domains.push_back(LabelUtils::StringToLabels(second_level_tld));
	auto query_rewrite = [d = std::move(allowed_domains)](const interpretation::Graph& graph, const interpretation::Graph::Path& p, moodycamel::ConcurrentQueue<json>& json_queue) {interpretation::Graph::Properties::QueryRewrite(graph, p, json_queue, d); };
	current_job_.path_functions.push_back(query_rewrite);

	current_job_.path_functions.push_back(interpretation::Graph::Properties::RewriteBlackholing);
}

void Driver::WriteViolationsToFile(string output_file) const
{
	//DumpNameServerZoneMap();
	Logger->info(fmt::format("Total number of violations: {}", property_violations_.size()));
	std::ofstream ofs;
	ofs.open(output_file, std::ofstream::out);
	ofs << "[\n";
	int c = 0;
	for (const json& j : property_violations_) {
		if (c != 0) ofs << ",\n";
		ofs << j.dump(4);
		c++;
	}
	ofs << "\n]";
	ofs.close();
	Logger->debug(fmt::format("driver.cpp (WriteViolationsToFile) - Output written to {}", output_file));
}


void Driver::DumpNameServerZoneMap() const {
	boost::unordered_map<int, string> zoneId_to_zone_name;

	for (auto const& [key, val] : context_.zoneId_to_zone) {
		zoneId_to_zone_name.insert({ key, LabelUtils::LabelsToString(val.get_origin()) });
	}
	json j = {};
	for (auto const& [ns, zoneIds] : context_.nameserver_zoneIds_map) {
		j[ns] = {};
		for (auto const& i : zoneIds) {
			j[ns].push_back(zoneId_to_zone_name.at(i));
		}
	}
	std::ofstream ofs;
	ofs.open("Graphs/nameserver_map.json", std::ofstream::out);
	ofs << j.dump(4);
	ofs.close();
}
