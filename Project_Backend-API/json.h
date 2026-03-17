#ifndef _JSON_H_
#define _JSON_H_

#include <fstream>
#include <iterator>
#include <string>
#include <json/json.h>

#ifdef _WIN32
#pragma comment(lib, "jsoncpp.lib")
#endif

Json::Value ReadJsonFromString(std::string jsonString) {
	Json::Value Result;
	Json::Reader().parse(jsonString, Result);
	return Result;
}

Json::Value readJsonFile(std::string fileName) {
	Json::Value Result;
	std::ifstream file(fileName);
	if (!file.is_open()) return Result;

	std::string jsonString((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	Json::Reader().parse(jsonString, Result);
	return Result;
}

void writeJsonFile(std::string fileName, Json::Value json) {
	std::ofstream file(fileName, std::ios::out | std::ios::trunc);
	if (!file.is_open()) return;

	file << Json::FastWriter().write(json);
}

#endif // !_JSON_H_