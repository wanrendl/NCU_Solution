#pragma once
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <Windows.h>

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

class TimeCalculator {
private:
	time_t timestamp;
	int hour;
	int minute;
	int second;
public:
	TimeCalculator();
	TimeCalculator(time_t ts);
	TimeCalculator(std::string timeStr);
	time_t GetSeconds() const;
	int GetHour() const;
	int GetMinute() const;
	int GetSecond() const;
	bool compare(const TimeCalculator& other) const;
	bool compare(const std::string& timeStr) const;
	bool compare(time_t ts) const;
	bool operator<(const TimeCalculator& other) const;
	bool operator<(const std::string& timeStr) const;
	bool operator>(const TimeCalculator& other) const;
	bool operator>(const std::string& timeStr) const;
	bool operator<=(const TimeCalculator& other) const;
	bool operator>=(const TimeCalculator& other) const;
	bool operator==(const TimeCalculator& other) const;
	bool operator==(time_t other) const;
	bool operator=(const std::string& timeStr);
	bool operator-=(int seconds);
	bool operator+=(int seconds);
	TimeCalculator operator-(int seconds) const;
	TimeCalculator operator+(int seconds) const;
	std::string print() const;
};

class CountdownTimer {
private:
	std::chrono::system_clock::time_point startTime;
	std::chrono::system_clock::time_point endTime;
	unsigned short timeZoneOffset = 0;
public:
	CountdownTimer(time_t endDate);
	CountdownTimer(std::string endTimeString);
	void setTimeZone(unsigned short timeZone);
	void begin();
	std::string print() const;
	bool isFinished() const;
};