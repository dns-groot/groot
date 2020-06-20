#ifndef DRIVER_TEST_H_
#define DRIVER_TEST_H_
#include "../src/driver.h"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

class DriverTest
{

  public:
    int GetNumberofLabelGraphVertices(Driver &d)
    {
        return static_cast<int>(num_vertices(d.label_graph_));
    }

    int GetNumberofLabelGraphEdges(Driver &d)
    {
        return static_cast<int>(num_edges(d.label_graph_));
    }

    int GetNumberofResourceRecordsParsed(Driver &d, string file, string nameserver)
    {
        return d.ParseZoneFileAndExtendGraphs(file, nameserver, "", false);
    }

    int GetNumberofViolations(Driver &d)
    {
        return static_cast<int>(d.property_violations_.size());
    }

    boost::unordered_map<string, long> GetTypeToCountMap(Driver &d)
    {
        return d.context_.type_to_rr_count;
    }

    const zone::Graph &GetLatestZone(Driver &d)
    {
        return d.context_.zoneId_to_zone.at(d.context_.zoneId_counter_);
    }

    interpretation::Graph CreateAnInterpretationGraph(Driver &d, const EC ec)
    {
        return interpretation::Graph(ec, d.context_);
    }

    void GenerateLabelGraphDotFile(Driver &d, string filepath)
    {
        d.label_graph_.GenerateDotFile(filepath);
    }
};

#endif