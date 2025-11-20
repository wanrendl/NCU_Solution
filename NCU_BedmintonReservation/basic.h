#pragma once
#include <chrono>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <sstream>

template<typename T>
static bool isIn(T content, std::vector<T> vec) {
	for (auto& it : vec) if (content == it) return true;
	return false;
}

std::string ReadStrngFromFile(std::string fileName) {
	std::ifstream file(fileName, std::ios::binary);
	std::stringstream sStream;

	sStream << file.rdbuf();
	file.close();
	return sStream.str();
}

std::string ParseStringPos(std::string str, size_t pos1, size_t pos2) {
	return str.substr(pos1, pos2 - pos1 + 1);
}

// 从 tm 结构体中提取各部分
//int year = local_tm->tm_year + 1900; // tm_year 从1900年算起
//int month = local_tm->tm_mon + 1;    // tm_mon 从0开始
//int day = local_tm->tm_mday;
//int hour = local_tm->tm_hour;
//int minute = local_tm->tm_min;
//int second = local_tm->tm_sec;

void DelayPrint(std::string s, int ms = 50) {
	for (auto& it : s) {
		std::cout << it;
		Sleep(ms);
	}
}

void ColorfulPrint(std::string s, WORD wAttributes) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleTextAttribute(hConsole, wAttributes);
	std::cout << s;
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 重置为默认颜色
}

class CurrentTime {
private:
	time_t currentTime;
	std::chrono::system_clock::time_point now;
public:
	time_t GetTimeStamp() {
		now = std::chrono::system_clock::now();
		currentTime = std::chrono::system_clock::to_time_t(now);
		return currentTime;
	}
	std::string GetFormattedTime() {
		now = std::chrono::system_clock::now();
		currentTime = std::chrono::system_clock::to_time_t(now);
		std::stringstream t;
		t << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S");
		return t.str();
	}
	std::string GetFormattedTimeAfter(int days) {
		now = std::chrono::system_clock::now();
		currentTime = std::chrono::system_clock::to_time_t(now) + static_cast<time_t>(days) * 24 * 60 * 60;
		std::stringstream t;
		t << std::put_time(std::localtime(&currentTime), "%Y-%m-%d");
		return t.str();
	}
	int GetHour() {
		now = std::chrono::system_clock::now();
		currentTime = std::chrono::system_clock::to_time_t(now);
		std::tm* local_tm = std::localtime(&currentTime);
		return local_tm->tm_hour;
	}
	int GetMinute() {
		now = std::chrono::system_clock::now();
		currentTime = std::chrono::system_clock::to_time_t(now);
		std::tm* local_tm = std::localtime(&currentTime);
		return local_tm->tm_min;
	}
};

static time_t StringToTimeStamp(std::string str) {
	struct tm tm_ {};
	int year, month, day;
	sscanf_s(str.c_str(), "%d-%d-%d", &year, &month, &day);
	tm_.tm_year = year - 1900;
	tm_.tm_mon = month - 1;
	tm_.tm_mday = day - 2;
	tm_.tm_hour = 12;
	tm_.tm_min = 0;
	tm_.tm_sec = 0;
	tm_.tm_isdst = 0;

	time_t timeStamp = mktime(&tm_);
	return timeStamp;
}