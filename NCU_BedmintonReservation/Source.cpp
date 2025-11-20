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
const std::string Version = "v20251120-151300";

const std::string NCU_VenueReservation_Login = "http://ndyy.ncu.edu.cn:8089/cas/login";

static std::string GenerateToken(std::string username, std::string password) {

	std::string NCU_user_token = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJleHAiOjE3NjM2MzU2NDUsInVzZXJOYW1lIjoi5p6X56uvIiwidXNlciI6IjU3MTYxMjUwNjEiLCJ0eXBlQ29kZSI6IlMwMiJ9.YP8K-sSyNjoJnifJ5NaslCCWig3BpTDtZfgHTcxfY_c";

	if (NCU_user_token.empty()) {

		const std::string fpVisitorId = "caac21938b004343dbe3191a9b433359";

		httplib::SSLClient client("cas.ncu.edu.cn", 8443);

		httplib::Client client_ndyy("ndyy.ncu.edu.cn", 8089);

		httplib::Result result_GET_login, result_NCU_mfa, result_NCU_final_login, result_NCU_user_token;

		do {
			result_GET_login = client.Get("/cas/login?service=" + NCU_VenueReservation_Login);
		} while (!result_GET_login);

		//std::cout << "Login Page Status: " << result_GET_login->status << std::endl;

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

		//std::cout << "NCU Execution Get Successfully." << std::endl << std::endl;

		httplib::Params param_mfa;
		param_mfa.emplace("username", username);
		param_mfa.emplace("password", password);
		param_mfa.emplace("fpVisitorId", fpVisitorId);

		do {
			result_NCU_mfa = client.Post("/cas/mfa/detect", param_mfa);
		} while (!result_NCU_mfa);

		//std::cout << "MFA Status: " << result_NCU_mfa->status << std::endl;

		std::string mfa_state = ReadJsonFromString(result_NCU_mfa->body)["data"]["state"].asString();

		//std::cout << "MFA State: ";
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

		//std::cout << "Ticket Status: " << result_NCU_final_login->status << std::endl;

		std::string string_NCU_ticketURL = result_NCU_final_login->get_header_value("Location");
		std::string NCU_login_ticket = ParseStringPos(string_NCU_ticketURL, string_NCU_ticketURL.find("ticket=") + 7, string_NCU_ticketURL.length());

		//std::cout << "Ticket: " << NCU_login_ticket << std::endl << std::endl;

		do {
			result_NCU_user_token = client_ndyy.Get("/cas/login?ticket=" + NCU_login_ticket);
		} while (!result_NCU_user_token);

		//std::cout << "Token Status: " << result_NCU_user_token->status << std::endl;

		std::string string_NCU_tokenURL = result_NCU_user_token->get_header_value("location");
		NCU_user_token = string_NCU_tokenURL.substr(string_NCU_tokenURL.find("token=") + 6);
	}

	//std::cout << "Token: ";
	//DelayPrint(NCU_user_token, 1);
	//std::cout << std::endl << std::endl;

	return NCU_user_token;
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

	do {
		result = client.Get("/api/badminton/saveReservationInformation" + ssURI.str(), headers);
	} while (!result);

	Json::Value ReservationResponse;

	std::ofstream osLog("ReservationLog.txt", std::ios::app);
	if (!osLog.is_open()) std::cerr << "Failed to Open Log File." << std::endl;
	osLog << "[" << curretntTime.GetFormattedTime() << "] "
		<< "Court: " << std::setw(2) << std::setfill('0') << rInfo.hallID
		<< ", Date: " << rInfo.date
		<< ", Time: " << std::setw(2) << std::setfill('0') << rInfo.rTime << ":00-"
		<< std::setw(2) << std::setfill('0') << rInfo.rTime + 1 << ":00"
		<< ": " << result->body << std::endl;

	if (Json::Reader().parse(result->body, ReservationResponse)) {
		std::cout << "Court: " << std::setw(2) << std::setfill('0') << rInfo.hallID
			<< ", Date: " << rInfo.date
			<< ", Time: " << std::setw(2) << std::setfill('0') << rInfo.rTime << ":00-"
			<< std::setw(2) << std::setfill('0') << rInfo.rTime + 1 << ":00"
			<< ": ";
		if (ReservationResponse["code"].asString() == "200") ColorfulPrint("Success\n", FOREGROUND_GREEN);
		else if (ReservationResponse["code"].asString() == "600") ColorfulPrint("Processing\n", FOREGROUND_RED);
		else if (ReservationResponse["code"].asString() == "601") ColorfulPrint("Reserved\n", FOREGROUND_RED);
		else ColorfulPrint("Unknown Reason\n", FOREGROUND_RED);
	}
	else ColorfulPrint("Json Parse Failed.\n", FOREGROUND_RED);
}

int main() {
	std::string username, password;
	std::cout << Name << " - " << Version << std::endl;

	std::cout << "Username: ";
	std::cin >> username;

	std::cout << "Password: ";
	std::cin >> password;

	std::vector<ReservationInfo> Reservation;

	std::string date;
	int hallID, rTime;

	//File Read Style: XXXX-XX-XX HallID rTime
	std::ifstream isReservationInfo("ReservationInfo.txt", std::ios::in);
	if (isReservationInfo.is_open()) while (isReservationInfo >> date >> hallID >> rTime) Reservation.push_back({ date, hallID, rTime, StringToTimeStamp(date)});
	else {
		std::cerr << "Failed to Load Reservation Info." << std::endl;
		return 0;
	}

	CurrentTime currentTime;

	httplib::SSLClient client_ndyy_ssl("ndyy.ncu.edu.cn");
	httplib::Result result_NCU_VenueInfo;

	std::string NCU_user_token;

	NCU_user_token = GenerateToken(username, password);

	std::cout << "Token: " << NCU_user_token << std::endl;

	std::vector<std::string> vector_abledDate;
	std::vector<Json::Value> vector_VenueJsonInfo;

	std::cout << "Current Time: " << currentTime.GetFormattedTime() << std::endl;

	if (currentTime.GetHour() >= 12) {
		std::cout << "Venue Reservation End Time: " << currentTime.GetFormattedTimeAfter(2) << std::endl;
		vector_abledDate.push_back(currentTime.GetFormattedTimeAfter(0));
		vector_abledDate.push_back(currentTime.GetFormattedTimeAfter(1));
		vector_abledDate.push_back(currentTime.GetFormattedTimeAfter(2));
	}
	else {
		std::cout << "Venue Reservation End Time: " << currentTime.GetFormattedTimeAfter(1) << std::endl;
		vector_abledDate.push_back(currentTime.GetFormattedTimeAfter(0));
		vector_abledDate.push_back(currentTime.GetFormattedTimeAfter(1));
	}

	std::stringstream sstream_sestr;

	httplib::Headers header_VenueInfo;
	header_VenueInfo.emplace("token", NCU_user_token);

	for (auto& it : vector_abledDate) {
		sstream_sestr << "?campus=%E5%89%8D%E6%B9%96%E5%8C%97%E9%99%A2&venues=%E5%85%A8%E9%83%A8%E5%9C%BA%E9%A6%86&types=%E7%BE%BD%E6%AF%9B%E7%90%83&date=" << it;
		do {
			result_NCU_VenueInfo = client_ndyy_ssl.Get("/api/badminton/areaReservationInformation" + sstream_sestr.str(), header_VenueInfo);
		} while (!result_NCU_VenueInfo);
		std::cout << "Get Date: " << it << ", Status Code: " << result_NCU_VenueInfo->status << std::endl;
		vector_VenueJsonInfo.push_back(ReadJsonFromString(result_NCU_VenueInfo->body));
		sstream_sestr.str("");
	}

	std::cout << std::endl;

	std::map<std::string, Json::Value> areaMapping;

	int c = 0;
	for (auto& it : vector_VenueJsonInfo) {
		areaMapping.emplace(vector_abledDate[c], it["data"]);
		std::cout << "===== Date: " << vector_abledDate[c] << " =====" << std::endl;
		for (auto& it_data : it["data"]) {
			std::cout << "Court " << std::setw(2) << std::setfill('0') << std::stoi(it_data["areaNickname"].asString().substr(4, 10)) << ": ";

			for (int i = 8; i <= 21; i += 1) {
				std::stringstream s;
				s << std::setw(2) << std::setfill('0') << i << ":00-" << std::setw(2) << std::setfill('0') << i + 1 << ":00 ";
				if (c == 0 && i + 1 < currentTime.GetHour()) ColorfulPrint(s.str(), FOREGROUND_RED);
				else if (it_data["time" + std::to_string(i)].asString() == "1") ColorfulPrint(s.str(), FOREGROUND_GREEN);
				else ColorfulPrint(s.str(), FOREGROUND_RED);
			}

			std::cout << std::endl;

		}
		c += 1;
	}

	std::cout << "=========== Over ===========" << std::endl << std::endl;
	
	std::cout << "Pausing...\r";

	while (currentTime.GetHour() != 11 && currentTime.GetMinute() != 45);
	std::cout << "Generating Token..." << std::endl;
	NCU_user_token = GenerateToken(username, password);

	while (currentTime.GetHour() != 12);
	std::cout << "Reservation..." << std::endl;

	std::vector<std::thread> ReservationThreads;

	for (auto& it : Reservation) {
		bool doContinue = false;
		
		std::cout << "Court: " << std::setw(2) << std::setfill('0') << it.hallID
			<< ", Date: " << it.date
			<< ", Time: " << std::setw(2) << std::setfill('0') << it.rTime << ":00-"
			<< std::setw(2) << std::setfill('0') << it.rTime + 1 << ":00"
			<< ", Reservation Status: ";
		
		if (!isIn(it.date, vector_abledDate)) {
			ColorfulPrint("Overtime\n", FOREGROUND_RED);
			doContinue = true;
		}
		
		if (it.hallID < 1 || it.hallID > 12) {
			ColorfulPrint("Overcount\n", FOREGROUND_RED);
			doContinue = true;
		}
		
		for (auto& it2 : areaMapping[it.date]) {
			if (it2["areaNickname"] == "hall" + std::to_string(it.hallID) && it2[std::string("time" + std::to_string(it.rTime))] != 1) {
				ColorfulPrint("Reserved\n", FOREGROUND_RED);
				doContinue = true;
			}
		}
		
		if (doContinue) continue;

		ReservationThreads.push_back(std::thread(AsyncReservation, it, NCU_user_token));

		ColorfulPrint("Reservation Information Verification Passed.\n", FOREGROUND_GREEN);
	}

	if (!ReservationThreads.empty()) for (auto& it : ReservationThreads) it.join();

	return 0;
}