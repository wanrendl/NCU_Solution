#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>

std::atomic<std::uint64_t> Logger::nextThreadNumber_{ 0 };

Logger::Logger(std::string_view filePath) {
	SetLogFile(filePath);
}

void Logger::SetLogFile(std::string_view filePath) {
	std::scoped_lock lock(mutex_);
	if (file_.is_open()) {
		file_.close();
	}
	file_.open(std::string(filePath), std::ios::app);
}

void Logger::EnableConsole(bool enable) {
	std::scoped_lock lock(mutex_);
	consoleEnabled_ = enable;
}

void Logger::Log(Level level, std::string_view message) {
	std::scoped_lock lock(mutex_);
	const std::string line = BuildLine(level, message);

	if (consoleEnabled_) {
     try {
			std::cout << line << std::endl;
		}
		catch (...) {
			consoleEnabled_ = false;
		}
	}

	if (file_.is_open()) {
     try {
			file_ << line << std::endl;
			file_.flush();
		}
		catch (...) {}
	}
}

void Logger::SetCurrentThreadName(std::string name) {
	threadLocalName() = std::move(name);
}

std::string Logger::BuildLine(Level level, std::string_view message) {
	const auto now = std::chrono::system_clock::now();
	const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
	std::tm tmValue{};

#ifdef _WIN32
	localtime_s(&tmValue, &nowTime);
#else
	localtime_r(&nowTime, &tmValue);
#endif

	const std::string threadName = GetThreadName();

	std::ostringstream oss;
	oss << "[" << std::put_time(&tmValue, "%Y-%m-%d %H:%M:%S") << "]"
		<< "[" << LevelToString(level) << "]"
        << "[" << std::this_thread::get_id() << "]"
		<< "[" << threadName << "] "
		<< message;
	return oss.str();
}

std::string Logger::GetThreadName() {
	auto& currentName = threadLocalName();
	if (!currentName.empty()) {
		return currentName;
	}

	const auto number = ++nextThreadNumber_;
	std::ostringstream oss;
	oss << "Thread-" << std::setw(2) << std::setfill('0') << number;
	currentName = oss.str();
	return currentName;
}

std::string& Logger::threadLocalName() {
	thread_local std::string name;
	return name;
}

const char* Logger::LevelToString(Level level) {
	switch (level) {
	case Level::Info: return "INFO";
	case Level::Warning: return "WARN";
	case Level::Error: return "ERROR";
	default: return "INFO";
	}
}