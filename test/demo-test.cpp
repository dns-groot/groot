#include "driver-test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(DemoTestSuite)

BOOST_AUTO_TEST_CASE(demo_test)
{
    Driver driver;
    DriverTest dt;
    boost::filesystem::path directory("TestFiles");
    std::ifstream metadataFile((directory / "cc.il.us" / "zone_files" / "metadata.json").string());
    json metadata;
    metadataFile >> metadata;

    std::ifstream i((directory / "cc.il.us" / "jobs.json").string());
    json j;
    i >> j;

    long total_rrs_parsed = driver.SetContext(metadata, (directory / "cc.il.us" / "zone_files").string(), false);

    BOOST_TEST(140 == total_rrs_parsed);
    BOOST_TEST(70 == dt.GetNumberofLabelGraphVertices(driver));
    BOOST_TEST(70 == dt.GetNumberofLabelGraphEdges(driver));

    boost::unordered_map<string, long> types_to_count = dt.GetTypeToCountMap(driver);

    BOOST_TEST(1 == types_to_count["Wildcard"]);
    BOOST_TEST(1 == types_to_count["DNAME"]);
    BOOST_TEST(5 == types_to_count["CNAME"]);

    long total_ecs = 0;

    for (auto &user_job : j) {
        driver.SetJob(user_job);
        driver.GenerateECsAndCheckProperties();
        total_ecs += driver.GetECCountForCurrentJob();
    }
    BOOST_TEST(176 == total_ecs);
    BOOST_TEST(20 == dt.GetNumberofViolations(driver));

    EC test_ec;
    test_ec.name = LabelUtils::StringToLabels("ds3.trial.cc.il.us.");
    test_ec.rrTypes.set(RRType::A);
    interpretation::Graph ig = dt.CreateAnInterpretationGraph(driver, test_ec);
    ig.GenerateDotFile("test_ig.dot"); // TODO: Check if file is generated properly
}

BOOST_AUTO_TEST_SUITE_END()