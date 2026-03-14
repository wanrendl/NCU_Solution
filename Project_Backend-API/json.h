#ifndef _JSON_H_
#define _JSON_H_

#include <json/json.h>

#pragma comment(lib, "jsoncpp.lib")

Json::Value ReadJsonFromString(std::string jsonString) {
	Json::Value Result;
	Json::Reader().parse(jsonString, Result);
	return Result;
}

#endif // !_JSON_H_