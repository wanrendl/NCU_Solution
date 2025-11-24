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

class CurrentTime {
private:
	time_t currentTime;
	std::chrono::system_clock::time_point now;
public:
	time_t GetTimeStamp();
	std::string GetFormattedTime();
	std::string GetFormattedTimeDate();
	std::string GetFormattedTimeAfter(int days);
	int GetHour();
	int GetMinute();
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
	bool operator+=(int days);
	DateCalculator operator+(int days) const;
private:
	bool isLeapYear(int y) const;
};

time_t StringToTimeStamp(std::string str);