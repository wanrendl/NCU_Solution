#pragma once
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <Windows.h>
#include <iomanip>

struct ReservationInfo {
	std::string date;
	int hallID;
	int rTime;
	time_t enableTimestamp;
};

template<typename T>
static bool isIn(T content, std::vector<T> vec) {
	for (auto& it : vec) if (content == it) return true;
	return false;
}

std::string ReadStrngFromFile(std::string fileName);

std::string ParseStringPos(std::string str, size_t pos1, size_t pos2);

void DelayPrint(std::string s, int ms);

void ColorfulPrint(std::string s, WORD wAttributes);

time_t StringToTimeStamp(const std::string timeStr);

class CurrentTime {
private:
	time_t currentTime;
	std::chrono::system_clock::time_point now;
public:
	CurrentTime();
	time_t GetSeconds();
	std::string GetFormattedTime();
	std::string GetFormattedTimeDate();
	std::string GetFormattedTimeAfter(int days);
	int GetHour();
	int GetMinute();
	int GetSecond();
	int GetMillisecond();
};

std::vector<std::string> StringSplit(std::string str, char delimiter);

class DateCalculator {
private:
	int year;
	int month;
	int day;
public:
	DateCalculator(std::string date);
	DateCalculator();
	bool setDate(std::string date);
	bool isValidDate() const;
	bool addDays(int days);
	bool addMonths(int months);
	bool addYears(int years);
	std::string Print() const;
	bool operator<(const DateCalculator& other) const;
	bool operator<(const std::string& date) const;
	bool operator>(const DateCalculator& other) const;
	bool operator>(const std::string& date) const;
	bool operator<=(const DateCalculator& other) const;
	bool operator>=(const DateCalculator& other) const;
	bool operator==(const DateCalculator& other) const;
	bool operator=(const std::string& date);
	bool operator-=(int days);
	DateCalculator operator-(int days) const;
	DateCalculator operator-(const DateCalculator& other) const;
	bool operator+=(int days);
	DateCalculator operator+(int days) const;
	DateCalculator operator+(const DateCalculator& other) const;
private:
	bool isLeapYear(int y) const;
};

class UTCTimer {
private:
	std::chrono::time_point<std::chrono::steady_clock> startTime;
	int timezoneOffsetHours = 0; // UTC offset in hours
public:
	UTCTimer() {
		startTime = std::chrono::steady_clock::now();
	}
	void setTimezoneOffset(int offsetHours) {
		timezoneOffsetHours = offsetHours;
	}
	std::string getCurrentTimeString() const {
		return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now());
	}
	std::string getCurrentDateString() const {
		return std::format("{:%Y-%m-%d}", std::chrono::system_clock::now());
	}
	std::string getCurrentClockString() const {
		return std::format("{:%H:%M:%S}", std::chrono::system_clock::now());
	}
	std::string getSpecificTimeString(time_t timestamp) const {
		return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::from_time_t(timestamp));
	}
};

class CountdownTimer {
private:
	std::chrono::seconds endTime;
	std::chrono::seconds startTime;
public:
	CountdownTimer();
	CountdownTimer(time_t endstamp);
	CountdownTimer(std::string timeString);
	CountdownTimer(std::string date, std::string clock);
	void begin();
	bool isFinished() const;
	int getRemainingSeconds() const;
	bool compare(std::string time_str);
	bool compare(std::string time_date, std::string time_clock);
	std::string getRemainingTimeString() const;
	std::chrono::seconds StringToTimeStamp(const std::string timeStr);
};