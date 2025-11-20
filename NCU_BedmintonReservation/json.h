#pragma once
#include <json/json.h>

#pragma comment(lib, "jsoncpp.lib")

Json::Value ReadJsonFromString(std::string jsonString) {
	Json::Value Result;
	Json::Reader().parse(jsonString, Result);
	return Result;
}