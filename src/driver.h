#ifndef DRIVER_H_
#define DRIVER_H_

#include <nlohmann/json.hpp>

#include "../concurrentqueue/concurrentqueue.h"

#include "context.h"
#include "equivalence-class.h"
#include "interpretation-properties.h"
#include "job.h"
#include "label-graph.h"
#include "zone-graph.h"

using json = nlohmann::json;

const int kECConsumerCount = 8;

class Driver
{

  private:
    label::Graph label_graph_;
    Context context_;
    Job current_job_;
    std::set<json> property_violations_;
    int ParseZoneFileAndExtendGraphs(string, string, string, bool);
    // void DumpNameServerZoneMap() const;

  public:
    friend class DriverTest;
    void GenerateECsAndCheckProperties();
    void GenerateAndOutputECs();
    long GetECCountForCurrentJob() const;
    long SetContext(const json &, string, bool = false);
    void SetJob(const json &);
    void SetJob(string &);
    void WriteStatsForAJob();
    void WriteViolationsToFile(string) const;
};

#endif