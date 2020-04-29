#include <boost/test/unit_test.hpp>
#include "driver-test.h"


BOOST_AUTO_TEST_SUITE(DemoTestSuite)

BOOST_AUTO_TEST_CASE(demo_test)
{
	Driver driver;
	DriverTest dt;
	boost::filesystem::path directory = dt.GetDemoDirectoryPath();
	std::ifstream metadataFile((directory / boost::filesystem::path{ "zone_files" }/ boost::filesystem::path{ "metadata.json" }).string());
	json metadata;
	metadataFile >> metadata;
	
	std::ifstream i((directory / "jobs.json").string());
	json j;
	i >> j;

	long total_rrs_parsed = driver.SetContext(metadata, (directory / boost::filesystem::path{ "zone_files" }).string());

	BOOST_TEST(101 == total_rrs_parsed);
	BOOST_TEST(43 == dt.GetNumberofLabelGraphVertices(driver));
	BOOST_TEST(43 == dt.GetNumberofLabelGraphEdges(driver));

	boost::unordered_map<string, long> types_to_count = dt.GetTypeToCountMap(driver);

	BOOST_TEST(1 == types_to_count["wildcard"]);
	BOOST_TEST(1 == types_to_count["DNAME"]);
	BOOST_TEST(3 == types_to_count["CNAME"]);

	long total_ecs = 0;

	for (auto& user_job : j) {
		driver.SetJob(user_job);
		driver.GenerateECsAndCheckProperties();
		total_ecs += driver.GetECCountForCurrentJob();
	}
	BOOST_TEST(120 == total_ecs);
	BOOST_TEST(56 == dt.GetNumberofViolations(driver));
}

BOOST_AUTO_TEST_SUITE_END()