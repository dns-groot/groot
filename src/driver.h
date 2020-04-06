#pragma once

#include <nlohmann/json.hpp>

#include "../concurrentqueue/concurrentqueue.h"

#include "context.h"
#include "equivalence-class.h"
#include "interpretation-properties.h"
#include "label-graph.h"
#include "job.h"
#include "zone-graph.h"

using json = nlohmann::json;

const int kECConsumerCount = 8;

class Driver {

private:
	label::Graph label_graph_;
	Context context_;
	Job current_job_;
	json property_violations_;
	int ParseZoneFileAndExtendGraphs(string file, string nameserver);

public:
	void CheckAllStructuralDelegations(string);
	void GenerateECsAndCheckProperties();
	long GetECCountForCurrentJob() const;
	void RemoveDuplicateViolations();
	void SetContext(const json&, string);
	void SetJob(const json&);
	void WriteViolationsToFile(string) const;
};
