#ifndef _NCU_RESERVATION_H_
#define _NCU_RESERVATION_H_

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif

#include "basic.h"
#include "httplib.h"
#include "json.h"
#include "Logger.h"
#include "sha256.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <vector>

struct ReservationInfo {
	std::string date;
	int court;
	int time;
	time_t reserveTime;
	std::string uniqueid;
};

struct OverdateReservationInfo {
	bool status;
	std::string info;
	std::string date;
	int court;
	int time;
	time_t reserveTime;
};

struct ReservationResult {
	bool status;
	int code;
	std::string info;
	ReservationInfo reservationInfo;
};

const std::string NCU_VenueReservation_Login = "http://ndyy.ncu.edu.cn:8089/cas/login";

class ReservationManager {
private:
	const std::string username = "5716125061";
	const std::string password = "qaqveQ-3xyzty-vudqut";
private:
	Logger& logger_;
private:
	std::string token;
	std::atomic<bool> generateToken{ false };
	std::atomic<bool> stopRequested_{ false };
	std::atomic<bool> started_{ false };
	std::queue<ReservationInfo> pendingReservations_;
	std::vector<ReservationInfo> ReservationProcessingQueue;
	std::vector<OverdateReservationInfo> ReservationFinishQueue;
	std::thread reserveProcessThread_;
	std::thread otherProcessThread_;
	std::mutex pendingReservationMutex_;
	std::condition_variable pendingReservationCv_;
	std::mutex processingQueueMutex;
	std::mutex finishedQueueMutex;
	std::mutex tokenMutex;
	std::mutex processingFileMutex;
	std::mutex finishedFileMutex;
	std::fstream processingQueueFile;
	std::fstream finishedQueueFile;
public:
	explicit ReservationManager(Logger& logger)
		: logger_(logger) {
	}

	~ReservationManager() {
		stopRequested_.store(true);
		pendingReservationCv_.notify_all();
		if (reserveProcessThread_.joinable()) {
			reserveProcessThread_.join();
		}
		if (otherProcessThread_.joinable()) {
			otherProcessThread_.join();
		}
	   {
			std::scoped_lock lock(processingFileMutex);
			if (processingQueueFile.is_open()) {
				processingQueueFile.close();
			}
		}
		{
			std::scoped_lock lock(finishedFileMutex);
			if (finishedQueueFile.is_open()) {
				finishedQueueFile.close();
			}
		}
	}

	void enableAutoPay() {
		//to be continued...
	}

	bool addReservation(std::string date, int court, int time, std::string& info, std::string& uniqueid) {
		DateCalculator dateRead = date;
		DateCalculator reserveDate = CurrentTime().GetFormattedTimeDate();
		if (!dateRead.isValidDate()) {
			info = "Invalid Date.";
			return false;
		}
		else if (dateRead < CurrentTime().GetFormattedTimeDate() || (dateRead == CurrentTime().GetFormattedTimeDate() && time + 1 < CurrentTime().GetHour())) {
			info = "Date is in the past.";
			return false;
		}
		std::string tokenSnapshot;
		{
			std::scoped_lock lock(tokenMutex);
			tokenSnapshot = token;
		}
		if (!CheckToken(tokenSnapshot, username)) tokenSnapshot = GenerateToken(username, password);
		{
			std::scoped_lock lock(tokenMutex);
			token = tokenSnapshot;
		}
		Json::Value table = GenerateCourtByDate(date, tokenSnapshot);
		if (table[court - 1][time - 8].asString() == "0") {
			info = "Court is reserved.";
			return false;
		}
		else if (table[court - 1][time - 8].asString() == "2") {
			info = "Court is used for teaching.";
			return false;
		}
		else if (table[court - 1][time - 8].asString() == "3") {
			info = "Court is overtime.";
			return false;
		}
		uniqueid = Sha256::Hex(date + std::to_string(court) + std::to_string(time) + std::to_string(rand() % 10000));
		if (CheckReservationExistence(uniqueid)) {
			info = "You have already reserved this court at this time.";
			return false;
		}


		if (CurrentTime().GetHour() >= 12) reserveDate += 2;
		else reserveDate += 1;

		if (dateRead <= reserveDate) {
			std::scoped_lock lock(pendingReservationMutex_);
			pendingReservations_.push({ date, court, time, 0, uniqueid });
			info = "Reservation will be made immediately.";
		}
		else {
			time_t reserveTime = StringToTimeStamp((dateRead - 2).Print()) + 12 * 3600 + 2;
			std::scoped_lock lock(pendingReservationMutex_);
			pendingReservations_.push({ date, court, time, reserveTime, uniqueid });
			writeProcessingFile({ date, court, time, reserveTime, uniqueid });
			info = "Reservation will be made at " + (dateRead - 2).Print() + " 12:00:00.";
		}
		pendingReservationCv_.notify_one();
		return true;
	}

	bool deleteReservation(std::string uniqueid) {
		std::scoped_lock lock(processingQueueMutex);
		for (auto it = ReservationProcessingQueue.begin(); it != ReservationProcessingQueue.end(); ++it) {
			if (it->uniqueid == uniqueid) {
				ReservationProcessingQueue.erase(it);
				removeProcessingFile(uniqueid);
				return true;
			}
		}
		return false;
	}

	bool getReservationTable(std::string date, Json::Value& table) {
		std::string tokenSnapshot;
		{
			std::scoped_lock lock(tokenMutex);
			tokenSnapshot = token;
		}
		if (!CheckToken(tokenSnapshot, username)) tokenSnapshot = GenerateToken(username, password);
		{
			std::scoped_lock lock(tokenMutex);
			token = tokenSnapshot;
		}
		table = GenerateCourtByDate(date, tokenSnapshot);
		return true;
	}

	std::vector<ReservationInfo> getReservationProcessingQueueSnapshot() {
		std::scoped_lock lock(processingQueueMutex);
		return ReservationProcessingQueue;
	}

	std::vector<OverdateReservationInfo> getReservationFinishQueueSnapshot() {
		std::scoped_lock lock(finishedQueueMutex);
		return ReservationFinishQueue;
	}

	void begin() {
		bool expected = false;
		if (!started_.compare_exchange_strong(expected, true)) {
			logger_.Warning("Reservation manager has already started.");
			return;
		}
		processingQueueFile.open("processingQueue", std::ios::in | std::ios::out | std::ios::app);
		finishedQueueFile.open("finishedQueue", std::ios::in | std::ios::out | std::ios::app);
		readProcessingFile();
		readFinishedFile();
		logger_.Info("Reservation manager is starting background workers.");
		reserveProcessThread_ = std::thread(&ReservationManager::processReservation, this);
		otherProcessThread_ = std::thread(&ReservationManager::processOther, this);
		generateToken.store(true);
	}

private:
	void processOther() {
		Logger::SetCurrentThreadName("Thread-Other");
		std::string tempToken;
		{
			std::scoped_lock lock(tokenMutex);
			tempToken = token;
		}
		while (!stopRequested_.load()) {
			if (generateToken.load()) { // If token generation is requested
				tempToken = GenerateToken(username, password);
				{
					std::scoped_lock lock(tokenMutex);
					token = tempToken;
				}
				generateToken.store(false);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void processReservation() {
		Logger::SetCurrentThreadName("Thread-Reservation");
		time_t lastCheckTime = CurrentTime().GetSeconds();
		std::vector<std::future<ReservationResult>> reservationFutures(24);
		int index = 0;
			auto pushFinishResult = [this](ReservationResult&& rResult) {
				std::scoped_lock finishLock(finishedQueueMutex);
				ReservationFinishQueue.push_back({
					rResult.status,
					rResult.info,
					rResult.reservationInfo.date,
					rResult.reservationInfo.court,
					rResult.reservationInfo.time,
					rResult.reservationInfo.reserveTime
				});
				addFinishedFile({
					rResult.status,
					rResult.info,
					rResult.reservationInfo.date,
					rResult.reservationInfo.court,
					rResult.reservationInfo.time,
					rResult.reservationInfo.reserveTime
				});
				removeProcessingFile(rResult.reservationInfo.uniqueid);
			};

			auto getTokenSnapshot = [this]() -> std::string {
				std::scoped_lock tokenLock(tokenMutex);
				return token;
			};

			auto acquireFutureSlot = [&]() -> size_t {
				while (reservationFutures[index].valid() &&
					reservationFutures[index].wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
					index = (index + 1) % reservationFutures.size();
				}
				return static_cast<size_t>(index);
			};

			auto drainReadyFutures = [&]() {
				for (auto& f : reservationFutures) {
					if (f.valid() && f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
						pushFinishResult(f.get());
					}
				}
			};

		while (!stopRequested_.load()) {
			if (CurrentTime().GetHour() >= 11 && CurrentTime().GetHour() < 12 && CurrentTime().GetMinute() >= 50) {
				if (CurrentTime().GetSeconds() - lastCheckTime >= 600) { // Check token validity every 10 minutes
					lastCheckTime = CurrentTime().GetSeconds();
					std::string tempToken;
					{
						std::scoped_lock lock(tokenMutex);
						tempToken = token;
					}
					if (tempToken.empty() || !CheckToken(tempToken, username)) tempToken = GenerateToken(username, password);
					{
						std::scoped_lock lock(tokenMutex);
						token = tempToken;
					}
				}
			}

			std::queue<ReservationInfo> pendingLocal;
			{
				std::unique_lock pendingLock(pendingReservationMutex_);
				pendingReservationCv_.wait_for(pendingLock, std::chrono::milliseconds(200), [this] {
					return stopRequested_.load() || !pendingReservations_.empty();
				});
				if (stopRequested_.load()) {
					break;
				}
				std::swap(pendingLocal, pendingReservations_);
			}

			while (!pendingLocal.empty()) {
				ReservationInfo newInfo = std::move(pendingLocal.front());
				pendingLocal.pop();

				if (newInfo.reserveTime == 0) {
					const auto slot = acquireFutureSlot();
					reservationFutures[slot] = std::async(
						std::launch::async,
						&ReservationManager::AsyncReservation,
						this,
						newInfo,
						getTokenSnapshot()
					);
					drainReadyFutures();
				}
				else {
					std::scoped_lock processingLock(processingQueueMutex);
					auto it = std::upper_bound(
						ReservationProcessingQueue.begin(),
						ReservationProcessingQueue.end(),
						newInfo.reserveTime,
						[](time_t t, const ReservationInfo& r) {
							return t < r.reserveTime;
						}
					);
					ReservationProcessingQueue.insert(it, std::move(newInfo));
				}
			}

			std::vector<ReservationInfo> dueNow;
			{
				std::scoped_lock processingLock(processingQueueMutex);
				const time_t now = time(nullptr);
				while (!ReservationProcessingQueue.empty() &&
					ReservationProcessingQueue.front().reserveTime <= now) {
					dueNow.push_back(std::move(ReservationProcessingQueue.front()));
					ReservationProcessingQueue.erase(ReservationProcessingQueue.begin());
				}
			}

			for (auto& rEnd : dueNow) {
				const auto slot = acquireFutureSlot();
				reservationFutures[slot] = std::async(
					std::launch::async,
					&ReservationManager::AsyncReservation,
					this,
					rEnd,
					getTokenSnapshot()
				);
			}
			drainReadyFutures();
		}
	}

private:
	bool readProcessingFile() {
	   std::scoped_lock fileLock(processingFileMutex);
		if (!processingQueueFile.is_open()) return false;
		processingQueueFile.clear();
		processingQueueFile.seekg(0, std::ios::beg);
		std::string line;
		while (std::getline(processingQueueFile, line)) {
			std::istringstream iss(line);
			ReservationInfo rInfo;
			if (!(iss >> rInfo.uniqueid >> rInfo.date >> rInfo.court >> rInfo.time >> rInfo.reserveTime)) {
				logger_.Warning("Failed to parse processing queue line: " + line);
				continue;
			}
			std::scoped_lock queueLock(processingQueueMutex);
			auto it = std::upper_bound(
				ReservationProcessingQueue.begin(),
				ReservationProcessingQueue.end(),
				rInfo.reserveTime,
				[](time_t t, const ReservationInfo& r) {
					return t < r.reserveTime;
				}
			);
			ReservationProcessingQueue.insert(it, std::move(rInfo));
		}
		processingQueueFile.clear();
		processingQueueFile.seekp(0, std::ios::end);
		return true;
	}
	bool writeProcessingFile(const ReservationInfo& rInfo) {
		std::scoped_lock lock(processingFileMutex);
		if (!processingQueueFile.is_open()) return false;
		processingQueueFile.clear();
		processingQueueFile.seekp(0, std::ios::end);

		std::ostringstream lineOss;
		lineOss << rInfo.uniqueid << " " << rInfo.date << " " << rInfo.court << " " << rInfo.time << " " << rInfo.reserveTime;
		const std::string line = lineOss.str();

		processingQueueFile << line << "\n";
		processingQueueFile.flush();
		logger_.Info("Write file [processingQueue]: " + line);
		return true;
	}
	bool removeProcessingFile(const std::string& uniqueid) {
		std::scoped_lock lock(processingFileMutex);
		if (!processingQueueFile.is_open()) return false;
		std::vector<ReservationInfo> tempQueue;
		std::string line;
	   processingQueueFile.clear();
		processingQueueFile.seekg(0, std::ios::beg);
		while (std::getline(processingQueueFile, line)) {
			std::istringstream iss(line);
			ReservationInfo rInfo;
			if (!(iss >> rInfo.uniqueid >> rInfo.date >> rInfo.court >> rInfo.time >> rInfo.reserveTime)) {
				logger_.Warning("Failed to parse processing queue line for deletion: " + line);
				continue;
			}
			if (rInfo.uniqueid != uniqueid) {
				tempQueue.push_back(std::move(rInfo));
			}
		}
		processingQueueFile.close();
		processingQueueFile.open("processingQueue", std::ios::trunc | std::ios::out);
	   if (!processingQueueFile.is_open()) return false;

		for (const auto& r : tempQueue) {
			std::ostringstream lineOss;
			lineOss << r.uniqueid << " " << r.date << " " << r.court << " " << r.time << " " << r.reserveTime;
			const std::string writeLine = lineOss.str();

			processingQueueFile << writeLine << "\n";
			logger_.Info("Write file [processingQueue]: " + writeLine);
		}

		processingQueueFile.flush();
		processingQueueFile.close();
		processingQueueFile.open("processingQueue", std::ios::in | std::ios::out | std::ios::app);
		if (!processingQueueFile.is_open()) return false;
		return true;
	}
	bool addFinishedFile(const OverdateReservationInfo& rInfo) {
		std::scoped_lock lock(finishedFileMutex);
		if (!finishedQueueFile.is_open()) return false;
		finishedQueueFile.clear();
		finishedQueueFile.seekp(0, std::ios::end);

		std::ostringstream lineOss;
		lineOss << rInfo.status << " " << rInfo.date << " " << rInfo.court << " " << rInfo.time << " " << rInfo.reserveTime << " " << rInfo.info;
		const std::string line = lineOss.str();

		finishedQueueFile << line << "\n";
		finishedQueueFile.flush();
		logger_.Info("Write file [finishedQueue]: " + line);
		return true;
	}
	bool readFinishedFile() {
	 std::scoped_lock fileLock(finishedFileMutex);
		if (!finishedQueueFile.is_open()) return false;
		finishedQueueFile.clear();
		finishedQueueFile.seekg(0, std::ios::beg);
		std::string line;
		while (std::getline(finishedQueueFile, line)) {
			std::istringstream iss(line);
			OverdateReservationInfo rInfo;
			if (!(iss >> rInfo.status >> rInfo.date >> rInfo.court >> rInfo.time >> rInfo.reserveTime)) {
				logger_.Warning("Failed to parse finished queue line: " + line);
				continue;
			}
		   std::getline(iss >> std::ws, rInfo.info);
			std::scoped_lock queueLock(finishedQueueMutex);
			ReservationFinishQueue.push_back(std::move(rInfo));
		}
		finishedQueueFile.clear();
		finishedQueueFile.seekp(0, std::ios::end);
		return true;
	}
	std::string GenerateToken(std::string username, std::string password) {
		std::string NCU_user_token;

		if (!NCU_user_token.empty()) return NCU_user_token;

		const std::string fpVisitorId = "caac21938b004343dbe3191a9b433359";

		httplib::SSLClient client("cas.ncu.edu.cn", 8443);
		httplib::Client client_ndyy("ndyy.ncu.edu.cn", 8089);

		httplib::Result result_GET_login, result_NCU_mfa, result_NCU_final_login, result_NCU_user_token;

		while (true) {
			do {
				result_GET_login = client.Get("/cas/login?service=" + NCU_VenueReservation_Login);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} while (!result_GET_login);

			if (result_GET_login->status != 200) {
				logger_.Warning("Login page request failed, retrying.");
				continue;
			}

			logger_.Info("Login page status: " + std::to_string(result_GET_login->status));

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

			logger_.Info("NCU execution token parsed successfully.");

			httplib::Params param_mfa;
			param_mfa.emplace("username", username);
			param_mfa.emplace("password", password);
			param_mfa.emplace("fpVisitorId", fpVisitorId);

			do {
				result_NCU_mfa = client.Post("/cas/mfa/detect", param_mfa);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} while (!result_NCU_mfa);

			if (result_NCU_mfa->status != 200) {
				logger_.Warning("MFA detection failed, retrying.");
				continue;
			}

			std::string mfa_state = ReadJsonFromString(result_NCU_mfa->body)["data"]["state"].asString();
			logger_.Info("MFA status/state: " + std::to_string(result_NCU_mfa->status) + "/" + mfa_state);

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
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} while (!result_NCU_final_login);

			if (result_NCU_final_login->status != 302) {
				logger_.Warning("Final login failed, retrying. status=" + std::to_string(result_NCU_final_login->status));
				continue;
			}

			std::string string_NCU_ticketURL = result_NCU_final_login->get_header_value("Location");
			std::string NCU_login_ticket = ParseStringPos(string_NCU_ticketURL, string_NCU_ticketURL.find("ticket=") + 7, string_NCU_ticketURL.length());
			logger_.Info("Final login success, ticket acquired.");

			do {
				result_NCU_user_token = client_ndyy.Get("/cas/login?ticket=" + NCU_login_ticket);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} while (!result_NCU_user_token);

			if (result_NCU_user_token->status != 302) {
				logger_.Warning("Token generation failed, retrying. status=" + std::to_string(result_NCU_user_token->status));
				continue;
			}

			std::string string_NCU_tokenURL = result_NCU_user_token->get_header_value("location");
			NCU_user_token = string_NCU_tokenURL.substr(string_NCU_tokenURL.find("token=") + 6);
			logger_.Info("Token generated successfully.");
			break;
		}

		return NCU_user_token;
	}

	bool CheckToken(std::string token, std::string username) {
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

	ReservationResult AsyncReservation(ReservationInfo rInfo, std::string token) {
		ReservationResult rResult;
		rResult.reservationInfo = rInfo;
		CurrentTime curretntTime;
		httplib::SSLClient client("ndyy.ncu.edu.cn");
		std::stringstream ssURI;
		ssURI << "?role=ROLE_STUDENT"
			<< "&date=" << rInfo.date
			<< "&startTime=" << std::setw(2) << std::setfill('0') << rInfo.time << ":00-" << std::setw(2) << std::setfill('0') << rInfo.time << ":00"
			<< "&areaName=%E7%BE%BD%E6%AF%9B%E7%90%83" << rInfo.court << "%E5%8F%B7%E5%9C%BA%E5%9C%B0"
			<< "&areaNickname=hall" << rInfo.court;
		httplib::Result result;
		httplib::Headers headers;
		headers.emplace("Token", token);

		Json::Value ReservationResponse;

		rResult.status = false;
		for (int times = 0; times < 2; times += 1) {
			do {
				result = client.Get("/api/badminton/saveReservationInformation" + ssURI.str(), headers);
			} while (!result);

			if (!Json::Reader().parse(result->body, ReservationResponse)) {
				logger_.Warning("Reservation response JSON parse error, retrying.");
				continue;
			}
			rResult.code = std::stoi(ReservationResponse["code"].asString());
			std::ostringstream attemptOss;
			attemptOss << "Try " << times + 1 << " - Court: " << std::setw(2) << std::setfill('0') << rInfo.court
				<< ", Date: " << rInfo.date
				<< ", Time: " << std::setw(2) << std::setfill('0') << rInfo.time << ":00-"
				<< std::setw(2) << std::setfill('0') << rInfo.time + 1 << ":00"
				<< ": ";
			if (ReservationResponse["code"].asString() == "200") {
				rResult.status = true;
				rResult.info = "Success";
				logger_.Info(attemptOss.str() + "Success");
				break;
			}
			else if (ReservationResponse["code"].asString() == "600") rResult.info = "Processing / ExcessiveFrequency / TemporaryRestriction";
			else if (ReservationResponse["code"].asString() == "601") rResult.info = "Reserved";
			else if (ReservationResponse["code"].asString() == "401") rResult.info = "Invalid Token";
			else rResult.info = "Unknown";
			logger_.Warning(attemptOss.str() + rResult.info);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return rResult;
	}
	bool CheckReservationExistence(std::string uniqueid) {
		std::scoped_lock lock(processingQueueMutex);
		for (const auto& r : ReservationProcessingQueue) {
			if (r.uniqueid == uniqueid) return true;
		}
		return false;
	}
	Json::Value GenerateCourtByDate(std::string date, std::string token) {
		httplib::SSLClient client("ndyy.ncu.edu.cn");
		httplib::Result result;
		httplib::Headers headers;
		headers.emplace("Token", token);
		std::stringstream ssDate;
		ssDate << "?campus=%E5%89%8D%E6%B9%96%E5%8C%97%E9%99%A2&venues=%E5%85%A8%E9%83%A8%E5%9C%BA%E9%A6%86&types=%E7%BE%BD%E6%AF%9B%E7%90%83&date=" << date;
		do {
			result = client.Get("/api/badminton/areaReservationInformation" + ssDate.str(), headers);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		} while (!result);
		logger_.Info("Get reservation table status: " + std::to_string(result->status));
		Json::Value courtInfo = ReadJsonFromString(result->body), resultJson;

		for (int i = 0; i < courtInfo["data"].size(); i += 1) {
			for (int j = 8; j <= 21; j += 1) {
				if (date == CurrentTime().GetFormattedTimeDate() && j <= CurrentTime().GetHour()) resultJson[i].append(3);
				else resultJson[i].append(std::stoi(courtInfo["data"][i][std::string("time") + std::to_string(j)].asString()));
			}
		}

		{
			std::scoped_lock queueLock(processingQueueMutex);
			for (const auto& r : ReservationProcessingQueue) {
				if (r.date != date) {
					continue;
				}
				const int courtIndex = r.court - 1;
				const int timeIndex = r.time - 8;
				if (courtIndex >= 0 && courtIndex < resultJson.size() &&
					timeIndex >= 0 && resultJson[courtIndex].isArray() && timeIndex < resultJson[courtIndex].size()) {
					resultJson[courtIndex][timeIndex] = 4;
				}
			}
		}
		return resultJson;
	}
};

#endif // !_NCU_RESERVATION_H_