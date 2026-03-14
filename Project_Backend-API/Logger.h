#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

class Logger {
public:
	enum class Level {
		Info,
		Warning,
		Error
	};

 Logger() = default;

	explicit Logger(std::string_view filePath);

	void SetLogFile(std::string_view filePath);

	void EnableConsole(bool enable);

	void Log(Level level, std::string_view message);

	void Info(std::string_view message) { Log(Level::Info, message); }
	void Warning(std::string_view message) { Log(Level::Warning, message); }
	void Error(std::string_view message) { Log(Level::Error, message); }

	static void SetCurrentThreadName(std::string name);

private:
	static std::string BuildLine(Level level, std::string_view message);
	static std::string GetThreadName();
	static std::string& threadLocalName();
	static const char* LevelToString(Level level);

private:
	static std::atomic<std::uint64_t> nextThreadNumber_;
	std::mutex mutex_;
	std::ofstream file_;
	bool consoleEnabled_{ true };
};

#endif // !_LOGGER_H_