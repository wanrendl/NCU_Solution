#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "httplib.h"
#include "json.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include "basic.h"
#include <sstream>
#include <fstream>
#include <future>
#include <thread>
#include <vector>
#include <time.h>

const std::string Name = "NCU Court Reservation";
const std::string Version = "v20251127-231400";

const std::string NCU_VenueReservation_Login = "http://ndyy.ncu.edu.cn:8089/cas/login";

static std::string GenerateToken(std::string username, std::string password) {

	std::string NCU_user_token;

	if (!NCU_user_token.empty()) return NCU_user_token;

	const std::string fpVisitorId = "caac21938b004343dbe3191a9b433359";

	httplib::SSLClient client("cas.ncu.edu.cn", 8443);

	httplib::Client client_ndyy("ndyy.ncu.edu.cn", 8089);

	httplib::Result result_GET_login, result_NCU_mfa, result_NCU_final_login, result_NCU_user_token;

	do {
		result_GET_login = client.Get("/cas/login?service=" + NCU_VenueReservation_Login);
	} while (!result_GET_login);

	std::cout << "Login Page Status: " << result_GET_login->status << std::endl;

	std::stringstream NCU_login_html(result_GET_login->body);

	std::string analy_NCU_login_html_string, NCU_execution_content;

	bool doAppend = false;

	while (getline(NCU_login_html, analy_NCU_login_html_string)) {
		if (analy_NCU_login_html_string.find("id=\"fm1\"") != -1) doAppend = true;

		if (doAppend) {
			int start_pos = 0;
			std::string temp;
			for (int i = 0; i < analy_NCU_login_html_string.length(); i += 1) {
				if (analy_NCU_login_html_string[i] == '<') start_pos = i;
				if (analy_NCU_login_html_string[i] == '>') {
					temp = ParseStringPos(analy_NCU_login_html_string, start_pos, i);
					if (temp.find("name=\"execution\"") != -1) NCU_execution_content = ParseStringPos(temp, temp.find("value=\"") + 7, temp.find("\"/>") - 1);
				}
			}
		}

		if (doAppend && analy_NCU_login_html_string.find("</el-form>") != -1) {
			break;
			doAppend = false;
		}

		analy_NCU_login_html_string.clear();
	}

	std::cout << "NCU Execution Get Successfully." << std::endl << std::endl;

	httplib::Params param_mfa;
	param_mfa.emplace("username", username);
	param_mfa.emplace("password", password);
	param_mfa.emplace("fpVisitorId", fpVisitorId);

	do {
		result_NCU_mfa = client.Post("/cas/mfa/detect", param_mfa);
	} while (!result_NCU_mfa);

	std::cout << "MFA Status: " << result_NCU_mfa->status << std::endl;

	std::string mfa_state = ReadJsonFromString(result_NCU_mfa->body)["data"]["state"].asString();

	std::cout << "MFA State: " << mfa_state << std::endl;
	//DelayPrint(mfa_state);
	//std::cout << std::endl << std::endl;

	httplib::Params param_login;
	param_login.emplace("username", username);
	param_login.emplace("password", password);
	param_login.emplace("fpVisitorId", fpVisitorId);
	param_login.emplace("mfaState", mfa_state);
	param_login.emplace("execution", NCU_execution_content);
	param_login.emplace("currentMenu", "1");
	param_login.emplace("failN", "0");
	param_login.emplace("captcha", "");
	param_login.emplace("geolocation", "");
	param_login.emplace("_eventId", "submit");
	param_login.emplace("submit", "Login1");

	do {
		result_NCU_final_login = client.Post("/cas/login?service=" + NCU_VenueReservation_Login, param_login);
	} while (!result_NCU_final_login);

	std::cout << "Ticket Status: " << result_NCU_final_login->status << std::endl;

	std::string string_NCU_ticketURL = result_NCU_final_login->get_header_value("Location");
	std::string NCU_login_ticket = ParseStringPos(string_NCU_ticketURL, string_NCU_ticketURL.find("ticket=") + 7, string_NCU_ticketURL.length());

	std::cout << "Ticket: " << NCU_login_ticket << std::endl << std::endl;

	do {
		result_NCU_user_token = client_ndyy.Get("/cas/login?ticket=" + NCU_login_ticket);
	} while (!result_NCU_user_token);

	std::cout << "Token Status: " << result_NCU_user_token->status << std::endl;

	std::string string_NCU_tokenURL = result_NCU_user_token->get_header_value("location");
	NCU_user_token = string_NCU_tokenURL.substr(string_NCU_tokenURL.find("token=") + 6);

	std::cout << "Token: " << NCU_user_token << std::endl;
	//DelayPrint(NCU_user_token, 1);
	//std::cout << std::endl << std::endl;

	return NCU_user_token;
}

static bool CheckToken(std::string token, std::string username) {
	httplib::SSLClient client("ndyy.ncu.edu.cn");
	httplib::Headers headers;
	headers.emplace("Token", token);
	httplib::Result result;
	std::stringstream ssUsername;
	ssUsername << "?userId=" << username;
	do {
		result = client.Get("/api/badminton/getUserPhone" + ssUsername.str(), headers);
	} while (!result);

	if (ReadJsonFromString(result->body)["code"].asString() != "200") return false;
	return true;
}

void AsyncReservation(ReservationInfo rInfo, std::string token) {
	CurrentTime curretntTime;
	httplib::SSLClient client("ndyy.ncu.edu.cn");
	std::stringstream ssURI;
	ssURI << "?role=ROLE_STUDENT"
		<< "&date=" << rInfo.date
		<< "&startTime=" << std::setw(2) << std::setfill('0') << rInfo.rTime << ":00-" << std::setw(2) << std::setfill('0') << rInfo.rTime << ":00"
		<< "&areaName=%E7%BE%BD%E6%AF%9B%E7%90%83" << rInfo.hallID << "%E5%8F%B7%E5%9C%BA%E5%9C%B0"
		<< "&areaNickname=hall" << rInfo.hallID;
	httplib::Result result;
	httplib::Headers headers;
	headers.emplace("Token", token);

	Json::Value ReservationResponse;

	std::ofstream osLog("ReservationLog.txt", std::ios::app);
	for (int times = 0; times < 3; times += 1) {
		do {
			result = client.Get("/api/badminton/saveReservationInformation" + ssURI.str(), headers);
		} while (!result);

		osLog << "[" << curretntTime.GetFormattedTime() << "] "
			<< "Court: " << std::setw(2) << std::setfill('0') << rInfo.hallID
			<< ", Date: " << rInfo.date
			<< ", Time: " << std::setw(2) << std::setfill('0') << rInfo.rTime << ":00-"
			<< std::setw(2) << std::setfill('0') << rInfo.rTime + 1 << ":00"
			<< ": " << result->body << std::endl;

		if (!Json::Reader().parse(result->body, ReservationResponse)) {
			std::cout << "Json Parse Error, Retrying..." << std::endl;
			continue;
		}

		std::cout << "Try " << times + 1 << " - " << "Court: " << std::setw(2) << std::setfill('0') << rInfo.hallID
			<< ", Date: " << rInfo.date
			<< ", Time: " << std::setw(2) << std::setfill('0') << rInfo.rTime << ":00-"
			<< std::setw(2) << std::setfill('0') << rInfo.rTime + 1 << ":00"
			<< ": ";
		if (ReservationResponse["code"].asString() == "200") {
			std::cout << "Success\n";
			break;
		}
		else if (ReservationResponse["code"].asString() == "600") std::cout << "Processing" << std::endl;
		else if (ReservationResponse["code"].asString() == "601") std::cout << "Reserved" << std::endl;
		else if (ReservationResponse["code"].asString() == "401") std::cout << "Invalid Token" << std::endl;
		else std::cout << "Unknown Reason: " << ReservationResponse["code"].asString() << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	osLog.close();
}

static std::map<std::string, Json::Value> GenerateMap(std::string token) {
	std::vector<std::string> vector_abledDate;

	if (CurrentTime().GetHour() >= 12) {
		vector_abledDate.push_back(CurrentTime().GetFormattedTimeDate());
		DateCalculator tempDate = CurrentTime().GetFormattedTimeDate();
		tempDate += 1;
		vector_abledDate.push_back(tempDate.Print());
		tempDate += 1;
		vector_abledDate.push_back(tempDate.Print());
	}
	else {
		vector_abledDate.push_back(CurrentTime().GetFormattedTimeDate());
		DateCalculator tempDate = CurrentTime().GetFormattedTimeDate();
		tempDate += 1;
		vector_abledDate.push_back(tempDate.Print());
	}

	std::map<std::string, Json::Value> result;

	for (auto& it : vector_abledDate) {
		static httplib::SSLClient client_ndyy_ssl("ndyy.ncu.edu.cn");
		httplib::Result result_NCU_VenueInfo;
		httplib::Headers header_VenueInfo;
		header_VenueInfo.emplace("token", token);
		std::stringstream sstream_sestr;
		sstream_sestr << "?campus=%E5%89%8D%E6%B9%96%E5%8C%97%E9%99%A2&venues=%E5%85%A8%E9%83%A8%E5%9C%BA%E9%A6%86&types=%E7%BE%BD%E6%AF%9B%E7%90%83&date=" << it;
		do {
			result_NCU_VenueInfo = client_ndyy_ssl.Get("/api/badminton/areaReservationInformation" + sstream_sestr.str(), header_VenueInfo);
		} while (!result_NCU_VenueInfo);
		result.emplace(it, ReadJsonFromString(result_NCU_VenueInfo->body));
	}

	return result;
}

static void autoReservation(std::string& token, std::vector<ReservationInfo>& Reservation, std::map<std::string, Json::Value>& areaMapping) {
	std::vector<std::thread> ReservationThreads;
	for (auto& it : Reservation) {
		bool doContinue = false;

		std::cout << "Court: " << std::setw(2) << std::setfill('0') << it.hallID
			<< ", Date: " << it.date
			<< ", Time: " << std::setw(2) << std::setfill('0') << it.rTime << ":00-"
			<< std::setw(2) << std::setfill('0') << it.rTime + 1 << ":00"
			<< ", Reservation Status: ";

		if (it.hallID < 1 || it.hallID > 12) {
			ColorfulPrint("Overcount\n", FOREGROUND_RED);
			doContinue = true;
		}
		for (auto& it2 : areaMapping[it.date]["data"]) {
			if (it2["areaNickname"] == "hall" + std::to_string(it.hallID) && it2[std::string("time" + std::to_string(it.rTime))] != "1") {
				ColorfulPrint("Reserved\n", FOREGROUND_RED);
				doContinue = true;
			}
		}

		if (doContinue) continue;

		ReservationThreads.push_back(std::thread(AsyncReservation, it, token));

		ColorfulPrint("Reservation Information Verification Passed.\n", FOREGROUND_GREEN);
	}

	if (!ReservationThreads.empty()) for (auto& it : ReservationThreads) it.join();

}

int main() {
	std::ofstream osLog("ReservationLog.txt", std::ios::app);
	osLog << "[" << CurrentTime().GetFormattedTime() << "] "
		<< "================ New Reservation Session ================" << std::endl;
	std::string username, password;
	std::cout << Name << " - " << Version << std::endl;

	//std::cout << "Username: ";
	//std::cin >> username;
	
	//std::cout << "Password: ";
	//std::cin >> password;

	username = "****";
	password = "****";

	std::vector<ReservationInfo> Reservation;

	std::string date;
	int hallID, rTime;

	//File Read Style: XXXX-XX-XX HallID rTime
	/*
	* Check the validation of date.
	* When over 12:00, can only reserve for today, tomorrow, and the day after tomorrow.
	* and before 12:00, can only reserve for today and tomorrow.
	* so I need to check the date and create a vector to store when to reserve.
	* and if the date is overdue, skip it and report error.
	*/

	std::map<DateCalculator, std::vector<ReservationInfo>> map_abledDate;
	std::vector<ReservationInfo> reserveNow;
	std::vector<DateCalculator> reserveDateList;

	DateCalculator dateRead, reserveDate;
	//std::cout << std::endl << "Reserve Date Reference: " << reserveDate.Print() << std::endl;
	std::ifstream isReservationInfo("ReservationInfo.txt", std::ios::in);
	if (isReservationInfo.is_open()) {
		while (isReservationInfo >> date >> hallID >> rTime) {
			dateRead = date;
			reserveDate = CurrentTime().GetFormattedTimeDate();
			if (!dateRead.isValidDate()) {
				std::cerr << "Invalid Date: " << date << std::endl << std::endl;
				continue;
			}
			if (dateRead < CurrentTime().GetFormattedTimeDate()) {
				std::cerr << "Overdue Date: " << date << std::endl << std::endl;
				continue;
			}

			if (CurrentTime().GetHour() >= 12) reserveDate += 2;
			else reserveDate += 1;

			if (dateRead <= reserveDate) {
				reserveNow.push_back({ date, hallID, rTime, StringToTimeStamp(date) });
				continue;
			}

			map_abledDate[dateRead - 2].push_back({ date, hallID, rTime, StringToTimeStamp(date) });
			if(!isIn(dateRead - 2, reserveDateList)) reserveDateList.push_back(dateRead - 2);
		}
	}
	else {
		std::cerr << "Failed to Load Reservation Info." << std::endl;
		return 0;
	}

	std::cout << "============ Reservation Info ============" << std::endl;
	if (!reserveNow.empty()) {
		std::cout << "Immediate Reservation Date:" << std::endl;
		for (auto& it : reserveNow) {
			std::cout << "  - Court: " << std::setw(2) << std::setfill('0') << it.hallID
				<< ", Time: " << std::setw(2) << std::setfill('0') << it.rTime << ":00-"
				<< std::setw(2) << std::setfill('0') << it.rTime + 1 << ":00"
				<< std::endl;
		}
	}

	if (!map_abledDate.empty()) {
		for (auto& it : map_abledDate) {
			std::cout << "Scheduled Reservation Date: " << it.first.Print() << std::endl;
			for (auto& it2 : it.second) {
				std::cout << "  - Court: " << std::setw(2) << std::setfill('0') << it2.hallID
					<< ", Time: " << std::setw(2) << std::setfill('0') << it2.rTime << ":00-"
					<< std::setw(2) << std::setfill('0') << it2.rTime + 1 << ":00"
					<< std::endl;
			}
		}
	}
	std::cout << "==========================================" << std::endl << std::endl;

	std::string NCU_user_token;

	std::cout << "Generating Token." << std::endl;
	NCU_user_token = GenerateToken(username, password);
	std::cout << "Token: " << NCU_user_token << std::endl;

	std::cout << "Current Time: " << CurrentTime().GetFormattedTime() << std::endl;
	std::map<std::string, Json::Value> areaMapping = GenerateMap(NCU_user_token);

	for (auto& it : areaMapping) {
		std::cout << "===== Date: " << it.first << " =====" << std::endl;
		for (auto& it_data : it.second["data"]) {
			std::cout << "Court " << std::setw(2) << std::setfill('0') << std::stoi(it_data["areaNickname"].asString().substr(4, 10)) << ": ";

			for (int i = 8; i <= 21; i += 1) {
				std::stringstream s;
				s << std::setw(2) << std::setfill('0') << i << ":00-" << std::setw(2) << std::setfill('0') << i + 1 << ":00 ";
				if (it == *areaMapping.begin() && i + 1 < CurrentTime().GetHour()) ColorfulPrint(s.str(), FOREGROUND_RED);
				else if (it_data["time" + std::to_string(i)].asString() == "1") ColorfulPrint(s.str(), FOREGROUND_GREEN);
				else ColorfulPrint(s.str(), FOREGROUND_RED);
			}
			std::cout << std::endl;
		}
	}

	if (!reserveNow.empty()) autoReservation(NCU_user_token, reserveNow, areaMapping);
	
	for (auto& it : reserveDateList) {
		std::cout << "Waiting for Date: " << it.Print() << std::endl;
		while (CurrentTime().GetFormattedTimeDate() < it.Print()) std::this_thread::sleep_for(std::chrono::seconds(30));

		std::cout << "Date: " << it.Print() << " Reached, Waiting for Hour" << std::endl;

		CountdownTimer countdownTimer(it.Print() + "12:00:00");
		countdownTimer.setTimeZone(8);
		countdownTimer.begin();
		while (!countdownTimer.isFinished()) {

			if (countdownTimer.print() == "11:55:00") {
				std::cout << "Generating Token." << std::endl;
				NCU_user_token = GenerateToken(username, password);
				std::cout << "Token Generated." << std::endl;
			}

			std::cout << "===== ETA: " << countdownTimer.print() << " =====\r";
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << "Start Reserving for Date: " << it.Print() << std::endl;
		autoReservation(NCU_user_token, map_abledDate[it], areaMapping);
	}

	std::cout << "=========== Over ===========" << std::endl << std::endl;
	return 0;
}