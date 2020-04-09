
#include <chrono>
#include <ctime>
#include <docopt/docopt.h>
#include <boost/filesystem.hpp>
#include <ratio>

#include "../src/driver.h"

using namespace std::chrono;


void demo(string directory, string properties, string output_file) {

	Logger->debug("groot.cpp - demo function called");
	Driver driver;
	
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	
	std::ifstream metadataFile((boost::filesystem::path{ directory } / boost::filesystem::path{ "metadata.json" }).string());
	json metadata;
	metadataFile >> metadata;
	Logger->debug("groot.cpp (demo) - Successfully read metadata.json file");
	std::ifstream i(properties);
	json j;
	i >> j;
	
	Logger->debug("groot.cpp (demo) - Successfully read properties.json file");
	driver.SetContext(metadata, directory);
	Logger->debug("groot.cpp (demo) - Label graph and Zone graphs built");

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
	cout << "Time to build label graph and zone graphs: " << time_span.count() << endl;
	
	for (auto& user_job : j) {
		driver.SetJob(user_job);
		Logger->debug(fmt::format("groot.cpp (demo) - Started property checking for {}", string(user_job["Domain"])));
		driver.GenerateECsAndCheckProperties();
		Logger->debug(fmt::format("groot.cpp (demo) - Finished property checking for {} with {} ECs", string(user_job["Domain"]),driver.GetECCountForCurrentJob()));
	}
	t2 = high_resolution_clock::now();
	time_span = duration_cast<duration<double>>(t2 - t1);
	cout << "Time to check all user jobs: " << time_span.count() << endl;	
	
	driver.RemoveDuplicateViolations();
	driver.WriteViolationsToFile(output_file);
}

static const char USAGE[] =
R"(groot 1.0
   
Groot is a static verification tool for DNS. Groot consumes
a collection of zone files along with a collection of user- 
defined properties and systematically checks if any input to
DNS can lead to a property violation for the properties.

Usage: groot [-hv] [--jobs=<jobs_file_as_json>] <zone_directory> [--output=<output_file>]

Options:
  -h --help     Show this help screen.
  -v --verbose  Print more information.  
  --version     Show groot version.
)";


int main(int argc, const char** argv)
{
	try {
		auto args = docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "groot 1.0");

		/* for (auto const& arg : args) {
			std::cout << arg.first << arg.second << std::endl;
		 }*/
		spdlog::init_thread_pool(8192, 1);

		auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		stdout_sink->set_level(spdlog::level::err);
		stdout_sink->set_pattern("[%x %H:%M:%S.%e] [thread %t] [%^%=7l%$] %v");

		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);
		file_sink->set_level(spdlog::level::trace);
		file_sink->set_pattern("[%x %H:%M:%S.%e] [thread %t] [%^%=7l%$] %v");

		std::vector<spdlog::sink_ptr> sinks{ stdout_sink, file_sink };
		auto logger = std::make_shared<spdlog::async_logger>("my_custom_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		//auto  logger = std::make_shared<spdlog::logger>("my_custom_logger", sinks.begin(), sinks.end());
		logger->flush_on(spdlog::level::trace);

		bool verbose = args.find("--verbose")->second.asBool();
		if (verbose) {
			logger->set_level(spdlog::level::trace);
		}
		else {
			logger->set_level(spdlog::level::debug);
		}
		spdlog::register_logger(logger);

		Logger->bind(spdlog::get("my_custom_logger"));

		string zone_directory;
		string jobs_file;
		auto z = args.find("<zone_directory>");
		if (!z->second)
		{
			Logger->critical(fmt::format("groot.cpp (main) - missing parameter <zone_directory>"));
			cout << USAGE[0];
			exit(EXIT_FAILURE);
		}
		else
		{
			zone_directory = z->second.asString();
		}

		auto p = args.find("--jobs");
		if (p->second)
		{
			jobs_file = p->second.asString();
		}

		p = args.find("--output");
		string output_file = "output.json";
		if (p->second)
		{
			output_file = p->second.asString();
		}

		// TODO: validate that the directory and property files exist
		//profiling_net();
		//bench(zone_directory, properties_file);
		//checkHotmailDomains(zone_directory, properties_file, output);
		//checkUCLADomains(zone_directory, properties_file, output);
		demo(zone_directory, jobs_file, output_file);
		Logger->debug("groot.cpp (main) - Finished checking all jobs");
		spdlog::shutdown();
		return 0;
	}
	catch (exception & e) {
		cout << "Exception:- " << e.what() << endl;
		spdlog::shutdown();
	}
}