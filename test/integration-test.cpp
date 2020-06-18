#include "driver-test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(IntegrationTestSuite)

BOOST_AUTO_TEST_CASE(integration_test)
{
    Driver driver;
    // DriverTest dt;
    fs::path directory("TestFiles");
    auto test_directory = directory / "integration_tests";

    fs::directory_iterator it{test_directory};
    while (it != fs::directory_iterator{}) {
        auto entry = *it++;
        if (fs::is_directory(entry)) {
            auto test_directory = entry.path();
            auto zone_path = test_directory / "zone_files";
            auto metadata_path = zone_path / "metadata.json";
            auto output_path = test_directory / "output.json";
            auto expected_path = test_directory / "output_expected.json";
            auto jobs_path = test_directory / "jobs.json";
            auto lint_expected_path = test_directory / "lint_expected.json";

            // read in the metadata file
            std::ifstream metadataFile(metadata_path.string());
            json metadata;
            metadataFile >> metadata;

            // read in the jobs file
            std::ifstream i(jobs_path.string());
            json j;
            i >> j;

            fstream fs;
            fs.open("lint.json", ios::out);
            fs << "[\n";
            fs.close();

            driver.SetContext(metadata, zone_path.string(), true);

            fs.open("lint.json", ios::app);
            fs << "\n]";
            fs.close();

            for (auto &user_job : j) {
                driver.SetJob(user_job);
                driver.GenerateECsAndCheckProperties();
            }

            driver.WriteViolationsToFile(output_path.string());

            // compare the actual output with the expected output
            std::ifstream actual(output_path.string());
            std::string s1((std::istreambuf_iterator<char>(actual)), std::istreambuf_iterator<char>());

            std::ifstream expected(expected_path.string());
            std::string s2((std::istreambuf_iterator<char>(expected)), std::istreambuf_iterator<char>());

            BOOST_CHECK_EQUAL(s1, s2);

            std::ifstream actual_lint("lint.json");
            json lint;
            actual_lint >> lint;
           
            BOOST_CHECK_EQUAL(lint[0]["Violation"], "CNAME AND OTHER TYPES");
            BOOST_CHECK_EQUAL(lint[1]["Violation"], "Missing Glue Record");
            BOOST_CHECK_EQUAL(lint.size(), 2);

            actual_lint.close();
            fs::remove("lint.json");
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()