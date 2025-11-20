#pragma once
#include <chrono>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <sstream>

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
	std::string GetFormattedTimeAfter(int days);
	int GetHour();
	int GetMinute();
};

time_t StringToTimeStamp(std::string str);