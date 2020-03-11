#pragma once
#include <iostream>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace std;

class MyLogger {

	std::shared_ptr<spdlog::logger> _logger;
	MyLogger() {}
public:
	static void bind(std::shared_ptr<spdlog::logger> logger) {
		if (logger == nullptr) {
			throw nullptr;
		}
		getInstance()->_logger = logger;
	}
	static MyLogger* getInstance() {
		static MyLogger instance;
		return &instance;
	}
	void Trace(std::string s) {
		if (_logger) {
			_logger->trace(s);
		}
	}
	void Debug(std::string s) {
		if (_logger) {
			_logger->debug(s);
		}
	}
	void Info(std::string s) {
		if (_logger != nullptr) {
			_logger->info(s);
		}
	}
	void Warn(std::string s) {
		if (_logger) {
			_logger->warn(s);
		}
	}
	void Error(std::string s) {
		if (_logger) {
			_logger->error(s);
		}
	}
	void Critical(std::string s) {
		if (_logger) {
			_logger->critical(s);
		}
	}
};

inline class MyLogger* Logger = MyLogger::getInstance();

//#define Logger MyLogger::getInstance()