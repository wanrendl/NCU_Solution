#include "Converter.h"
#include "base64.h"
#include "databaseConnection.h"
#include "HTTPBackend.h"
#include "json.h"
#include "ncmmeta.h"
#include "NCUReservation.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string_view>

std::string Version = "v20260322-113000";

/*
sudo apt update
sudo apt install -y \
	build-essential g++ make pkg-config cmake \
	libjsoncpp-dev libssl-dev libtag1-dev libmysqlcppconn-dev zlib1g-dev

g++ -std=c++20 -O2 -DCPPHTTPLIB_OPENSSL_SUPPORT \
	Source.cpp HTTPBackend.cpp Logger.cpp basic.cpp sha256.cpp ncmmeta.cpp \
	-o Project_Backend-API \
	$(pkg-config --cflags --libs jsoncpp taglib) \
	-lmysqlcppconn -lssl -lcrypto -lpthread

nohup ./Project_Backend-API > backend.out 2>&1 &

tail -f backend.out

ps -ef | grep Project_Backend-API
*/

int main() {
	try {
		Logger logger;
		logger.SetLogFile("server.log");
		logger.EnableConsole(true);
		Logger::SetCurrentThreadName("Thread-0");

		DatabaseConnection dbConn("tcp://***:3306", "***", "***", "***");
		dbConn.setLogger(logger);
		dbConn.initialize();

		ReservationManager reservation(logger, dbConn);
		//reservation.begin();

		NeteaseConverter nConverter(dbConn, logger);
		nConverter.initialize();

		HttpServer server(8027, logger);
		std::mutex convertRawFileMutex;

		auto handleReservationTaskAdd = [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string date = requestJson["date"].asString();
			std::string court = requestJson["court"].asString();
			std::string time = requestJson["time"].asString();

			Json::Value responseJson;

			std::string info, uniqueid;

			if (reservation.addReservation(date, std::stoi(court), std::stoi(time), info, uniqueid)) {
				responseJson["success"] = true;
				responseJson["task"]["date"] = date;
				responseJson["task"]["court"] = court;
				responseJson["task"]["time"] = time;
				responseJson["task"]["uniqueid"] = uniqueid;
			}
			else responseJson["success"] = false;

			responseJson["message"] = info;

			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleReservationTaskRefresh = [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson, requestJson = ReadJsonFromString(req.body);
			auto maskMiddle = [](const std::string& input, size_t keepLeft, size_t keepRight, char maskChar = '*', size_t maskLength = 0) -> std::string {
				if (input.empty()) {
					return "";
				}
				if (input.size() <= keepLeft + keepRight) {
					return std::string(input.size(), maskChar);
				}
				std::string masked = input;
				for (size_t i = keepLeft; i < masked.size() - keepRight; ++i) {
					masked[i] = maskChar;
				}
				return masked;
				};
			responseJson["success"] = true;
			if (requestJson["type"].asString() == "processing") {
				responseJson["type"] = "processing";
				const auto processing = reservation.getReservationProcessingQueueSnapshot();
				for (auto it = processing.begin(); it != processing.end(); ++it) {
					Json::Value taskJson;
					taskJson["date"] = it->date;
					taskJson["court"] = it->court;
					taskJson["time"] = it->time;
					taskJson["uniqueid"] = it->uniqueid;
					taskJson["content"] = "Reservation will be made at " + (DateCalculator(it->date) - 2).Print() + " 12:00:00.";
					responseJson["data"].append(taskJson);
				}
			}
			else if (requestJson["type"].asString() == "finished") {
				responseJson["type"] = "finished";
				const auto finished = reservation.getReservationFinishQueueSnapshot();
				for (auto it = finished.begin(); it != finished.end(); ++it) {
					Json::Value taskJson;
					taskJson["status"] = it->status ? "Success" : "Failed";
					taskJson["info"] = it->info;
					taskJson["date"] = it->date;
					taskJson["court"] = it->court;
					taskJson["time"] = it->time;
					responseJson["data"].append(taskJson);
				}
			}
			else if (requestJson["type"].asString() == "table") {
				responseJson["type"] = "table";

				DateCalculator date(requestJson["date"].asString());
				if (!date.isValidDate()) {
					responseJson["success"] = false;
					responseJson["message"] = "Invalid date.";
				}
				else {
					Json::Value table;
					if (reservation.getReservationTable(date.Print(), table)) {
						responseJson["data"]["table"] = table;
					}
					else {
						responseJson["success"] = false;
						responseJson["message"] = "Failed to get reservation table.";
					}
				}
			}
			else if (requestJson["type"].asString() == "loginstat") {
				if (requestJson["checktoken"].asBool() == true) reservation.checkTokenValidate();
				loginInfo info;
				reservation.getloginInfo(info);

				info.token = info.token.substr(0, 4) + "**********" + info.token.substr(info.token.size() > 6 ? info.token.size() - 6 : 0);
				info.username = maskMiddle(info.username, 1, 3);

				responseJson["type"] = "loginstat";
				responseJson["data"]["validate"] = info.validate;
				responseJson["data"]["token"] = info.token;
				responseJson["data"]["username"] = info.username;
				responseJson["data"]["lastUpdateTime"] = info.lastUpdateTime;
				responseJson["data"]["lastCheckTime"] = info.lastCheckTime;
			}
			else if (requestJson["type"].asString() == "pendingpayment") {
				responseJson["type"] = "pendingpayment";
				const auto pendingPayment = reservation.getReservationPendingPayment();
				for (const auto& [uniqueid, paymentInfo] : pendingPayment) {
					Json::Value paymentJson;
					paymentJson["date"] = paymentInfo.date;
					paymentJson["court"] = paymentInfo.court;
					paymentJson["time"] = paymentInfo.time;
					paymentJson["reservationId"] = paymentInfo.reservationId;
					paymentJson["uniqueid"] = uniqueid;
					responseJson["data"].append(paymentJson);
				}
			}
			else {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid type: " + requestJson["type"].asString();
			}
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleReservationTaskDelete = [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string uniqueid = requestJson["uniqueid"].asString();

			Json::Value responseJson;

			if (reservation.deleteReservation(uniqueid)) {
				responseJson["success"] = true;
				responseJson["message"] = "Reservation: " + uniqueid + " deleted successfully.";
			}
			else {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete reservation: " + uniqueid + ".";
			}

			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleReservationAutoPayStatus = [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value config = readJsonFile("config.json");
			Json::Value responseJson;
			responseJson["success"] = true;
			responseJson["autopay"] = config.isObject() && config.isMember("autopay")
				? config["autopay"].asBool()
				: false;
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleReservationAutoPayChange = [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			Json::Value config = readJsonFile("config.json");
			const bool change = requestJson.isObject() && requestJson.isMember("change")
				? requestJson["change"].asBool()
				: false;
			config["autopay"] = change;
			writeJsonFile("config.json", config);

			Json::Value responseJson;
			responseJson["success"] = true;
			responseJson["autopay"] = config["autopay"].asBool();
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleReservationPendingDelete = [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string uniqueid = requestJson["uniqueid"].asString();
			Json::Value responseJson;
			if (reservation.removePendingPayment(uniqueid)) {
				responseJson["success"] = true;
				responseJson["message"] = "Pending payment: " + uniqueid + " deleted successfully.";
			} else {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete pending payment: " + uniqueid + ".";
			}
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleReservationPendingPay = [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			
		};

		auto handleConvertUpload = [&logger, &nConverter, &convertRawFileMutex](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson;

			auto sanitizeFilename = [](const std::string& input) -> std::string {
				if (input.empty()) {
					return "";
				}

				size_t pos = input.find_last_of("\\/");
				std::string base = (pos == std::string::npos) ? input : input.substr(pos + 1);

				base.erase(std::remove(base.begin(), base.end(), '\0'), base.end());
				return base;
			};

			auto getLowerExt = [](const std::string& fileName) -> std::string {
				size_t dot = fileName.find_last_of('.');
				if (dot == std::string::npos || dot + 1 >= fileName.size()) {
					return "";
				}

				std::string ext = fileName.substr(dot);
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
					return static_cast<char>(std::tolower(c));
					});
				return ext;
			};

			auto isNCMFile = [](const std::string& fileContent) -> bool {
			   static constexpr std::string_view kNcmMagic = "CTENFDAM";
				return std::string_view(fileContent).starts_with(kNcmMagic);
			};

			auto getHeaderIgnoreCase = [&req](const std::string& name) -> std::string {
				for (const auto& [k, v] : req.headers) {
					std::string lowerKey = k;
					std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), [](unsigned char c) {
						return static_cast<char>(std::tolower(c));
						});
					std::string lowerName = name;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
						return static_cast<char>(std::tolower(c));
						});
					if (lowerKey == lowerName) return v;
				}
				return "";
			};

			const std::string contentType = getHeaderIgnoreCase("Content-Type");
			const size_t boundaryPos = contentType.find("boundary=");
			if (boundaryPos == std::string::npos) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid upload request: missing multipart boundary.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			std::string boundary = contentType.substr(boundaryPos + 9);
			if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
				boundary = boundary.substr(1, boundary.size() - 2);
			}

			const std::string delimiter = "--" + boundary;
			const size_t partStart = req.body.find(delimiter);
			if (partStart == std::string::npos) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid multipart body.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			size_t headersStart = partStart + delimiter.size();
			if (req.body.compare(headersStart, 2, "\r\n") == 0) headersStart += 2;
			const size_t headersEnd = req.body.find("\r\n\r\n", headersStart);
			if (headersEnd == std::string::npos) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid multipart headers.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			const std::string partHeaders = req.body.substr(headersStart, headersEnd - headersStart);
			const size_t filenamePos = partHeaders.find("filename=");
			if (filenamePos == std::string::npos) {
				responseJson["success"] = false;
				responseJson["message"] = "No file found in request.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			size_t fileNameStart = filenamePos + 9;
			std::string fileName;
			if (fileNameStart < partHeaders.size() && partHeaders[fileNameStart] == '"') {
				++fileNameStart;
				const size_t quoteEnd = partHeaders.find('"', fileNameStart);
				if (quoteEnd == std::string::npos) {
					responseJson["success"] = false;
					responseJson["message"] = "Invalid filename in multipart request.";
					res.SendJson(Json::FastWriter().write(responseJson), 400);
					return;
				}
				fileName = partHeaders.substr(fileNameStart, quoteEnd - fileNameStart);
			}
			else {
				const size_t valueEnd = partHeaders.find(';', fileNameStart);
				fileName = partHeaders.substr(fileNameStart, valueEnd - fileNameStart);
			}

			fileName = sanitizeFilename(fileName);
			std::string ext = getLowerExt(fileName);

			if (ext != ".ncm") {
				responseJson["success"] = false;
				responseJson["message"] = "Only .ncm files are allowed.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			const size_t fileDataStart = headersEnd + 4;
			const size_t fileDataEnd = req.body.find("\r\n" + delimiter, fileDataStart);
			if (fileDataEnd == std::string::npos || fileDataEnd < fileDataStart) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid multipart file content.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			const std::string fileContent = req.body.substr(fileDataStart, fileDataEnd - fileDataStart);
			const std::filesystem::path outputDir = std::filesystem::path("convert") / "raw";
			std::filesystem::create_directories(outputDir);

			if (!isNCMFile(fileContent)) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid NCM file header.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			std::string fileHash = Sha256::Hex(fileContent);

			if (nConverter.searchPendingExists(fileHash)) {
				responseJson["success"] = false;
				responseJson["message"] = "A file with the same content has already been uploaded and is pending conversion.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			const std::filesystem::path outputPath = outputDir / fileHash;

			{
				std::scoped_lock lock(convertRawFileMutex);
				std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
				if (!output.is_open()) {
					responseJson["success"] = false;
					responseJson["message"] = "Failed to save uploaded file.";
					res.SendJson(Json::FastWriter().write(responseJson), 500);
					return;
				}

				output.write(fileContent.data(), static_cast<std::streamsize>(fileContent.size()));
				output.close();
			}

			logger.Info("Write file: " + outputPath.string() + " completed.");

			responseJson["success"] = true;
			responseJson["message"] = "File uploaded successfully.";
			responseJson["filename"] = fileName;

			if (!nConverter.addPendingDatabase(fileName.substr(0, fileName.size() - 4), fileHash, fileContent.size())) {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to add pending conversion to database.";
				{
					std::scoped_lock lock(convertRawFileMutex);
					std::error_code ec;
					std::filesystem::remove(outputPath, ec);
					if (ec) {
						logger.Error("Failed to delete file after database error: " + outputPath.string() + ". Error: " + ec.message());
					}
				}
				res.SendJson(Json::FastWriter().write(responseJson), 500);
				return;
			}
			nConverter.addPendingMap(fileHash, fileName);

			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleConvertRefresh = [&nConverter](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson;
			responseJson["success"] = true;
			const auto pending = nConverter.getConverterPending();
			for (const auto& record : pending) {
				if (record.at("status") == "1") continue;
				Json::Value pendingJson;
				pendingJson["filename"] = record.at("file_name");
				pendingJson["uniqueid"] = record.at("unique_id");
				pendingJson["filesize"] = record.at("file_size");
				responseJson["data"].append(pendingJson);
			}
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleConverterPendingDelete = [&nConverter, &convertRawFileMutex](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string uniqueid = requestJson["uniqueid"].asString();
			Json::Value responseJson;
			auto tryRemoveFileWithRetry = [](const std::filesystem::path& filePath, std::string& errorMessage) {
				if (!std::filesystem::exists(filePath)) {
					return true;
				}

				for (int i = 0; i < 6; ++i) {
					std::error_code ec;
					if (std::filesystem::remove(filePath, ec)) {
						return true;
					}

					if (!ec) {
						return !std::filesystem::exists(filePath);
					}

					errorMessage = ec.message();
					std::this_thread::sleep_for(std::chrono::milliseconds(120));
				}

				return false;
			};

			std::filesystem::path filePath = std::filesystem::path("convert") / "raw" / uniqueid;
			std::string removeFileError;
			{
				std::scoped_lock lock(convertRawFileMutex);
				if (!tryRemoveFileWithRetry(filePath, removeFileError)) {
					responseJson["success"] = false;
					responseJson["message"] = "Failed to delete file: " + filePath.string() + ". Error: " + removeFileError;
					res.SendJson(Json::FastWriter().write(responseJson));
					return;
				}
			}

			if (nConverter.removePendingDatabase(uniqueid)) {
				nConverter.removePendingMap(uniqueid);
				responseJson["success"] = true;
				responseJson["message"] = "Pending conversion: " + uniqueid + " deleted successfully.";
			}
			else {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete pending conversion: " + uniqueid + ".";
			}

			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleConvert = [&nConverter, &logger, &convertRawFileMutex](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			try {
				Json::Value requestJson = ReadJsonFromString(req.body);
				Json::Value responseJson;
				std::string fileHash = requestJson["uniqueid"].asString(), pictureHash;

				if (!nConverter.searchPendingExists(fileHash)) {
					responseJson["success"] = false;
					responseJson["message"] = "No pending conversion found for uniqueid: " + fileHash + ".";
					res.SendJson(Json::FastWriter().write(responseJson));
					return;
				}

				std::filesystem::path inputPath = std::filesystem::path("convert") / "raw" / fileHash;
				if (!std::filesystem::exists(inputPath) || !std::filesystem::is_regular_file(inputPath)) {
					responseJson["success"] = false;
					responseJson["message"] = "Source file not found: " + inputPath.string();
					res.SendJson(Json::FastWriter().write(responseJson));
					return;
				}

				size_t convertedFileSize = 0;
				int format = 0;
				auto tryRemoveFileWithRetry = [](const std::filesystem::path& filePath, std::string& errorMessage) {
					for (int i = 0; i < 6; ++i) {
						std::error_code ec;
						if (std::filesystem::remove(filePath, ec)) {
							return true;
						}

						if (!ec) {
							return !std::filesystem::exists(filePath);
						}

						errorMessage = ec.message();
						std::this_thread::sleep_for(std::chrono::milliseconds(120));
					}

					return false;
				};

				{
					std::scoped_lock fileLock(convertRawFileMutex);
					NeteaseCrypt crypt(inputPath.string());
					format = crypt.Dump("./convert/result", convertedFileSize);
					crypt.FixMetadata();
					nConverter.exportCover(format, "./convert/result/" + fileHash, (std::filesystem::path("convert") / "cover_images").string(), pictureHash);
				}
				{
					std::filesystem::path filePath = std::filesystem::path("convert") / "raw" / fileHash;
					if (std::filesystem::exists(filePath)) {
						std::string removeFileError;
						if (!tryRemoveFileWithRetry(filePath, removeFileError)) {
							responseJson["success"] = false;
							responseJson["message"] = "Failed to delete file: " + filePath.string() + ". Error: " + removeFileError;
							res.SendJson(Json::FastWriter().write(responseJson));
							return;
						}
					}
				}

				if (!nConverter.changePendingStatus(fileHash, "1")) {
					responseJson["success"] = false;
					responseJson["message"] = "Failed to update pending status.";
					res.SendJson(Json::FastWriter().write(responseJson));
					return;
				}

				std::string rawFileName = nConverter.getPendingFilename(fileHash);
				if (!nConverter.addFinishedDatabase(rawFileName, fileHash, format, pictureHash, convertedFileSize)) {
					responseJson["success"] = false;
					responseJson["message"] = "Failed to add finished conversion to database.";
					res.SendJson(Json::FastWriter().write(responseJson));
					return;
				}

				responseJson["success"] = true;
				responseJson["message"] = "File converted successfully.";
				res.SendJson(Json::FastWriter().write(responseJson));
			}
			catch (const std::exception& ex) {
				Json::Value responseJson;
				responseJson["success"] = false;
				responseJson["message"] = std::string("Convert failed: ") + ex.what();
				res.SendJson(Json::FastWriter().write(responseJson), 500);
			}
			catch (...) {
				Json::Value responseJson;
				responseJson["success"] = false;
				responseJson["message"] = "Convert failed: unknown exception.";
				res.SendJson(Json::FastWriter().write(responseJson), 500);
			}
		};
		auto handleConvertedRefresh = [&nConverter](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson;
			responseJson["success"] = true;
			const auto converted = nConverter.getConverted();
			for (const auto& record : converted) {
				Json::Value convertedJson;
				convertedJson["filename"] = record.at("file_raw_name");
				convertedJson["uniqueid"] = record.at("file_name");
				convertedJson["format"] = record.at("file_format");
				convertedJson["picture_hash"] = record.at("picture_hash");
				convertedJson["file_size"] = std::stoi(record.at("file_size"));
				responseJson["data"].append(convertedJson);
			}
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleConvertedDelete = [&nConverter](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string uniqueid = requestJson["uniqueid"].asString();
			Json::Value responseJson;
			if (uniqueid.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid uniqueid.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			std::string format;
			std::string pictureHash;
			for (const auto& record : nConverter.getConverted()) {
				auto itUnique = record.find("file_name");
				if (itUnique != record.end() && itUnique->second == uniqueid) {
					auto itFormat = record.find("file_format");
					auto itPicture = record.find("picture_hash");
					if (itFormat != record.end()) format = itFormat->second;
					if (itPicture != record.end()) pictureHash = itPicture->second;
					break;
				}
			}

			auto tryRemoveFileWithRetry = [](const std::filesystem::path& filePath, std::string& errorMessage) {
				if (!std::filesystem::exists(filePath)) {
					return true;
				}

				for (int i = 0; i < 6; ++i) {
					std::error_code ec;
					if (std::filesystem::remove(filePath, ec)) {
						return true;
					}

					if (!ec) {
						return !std::filesystem::exists(filePath);
					}

					errorMessage = ec.message();
					std::this_thread::sleep_for(std::chrono::milliseconds(120));
				}

				return false;
			};

			std::string lowerFormat = format;
			std::transform(lowerFormat.begin(), lowerFormat.end(), lowerFormat.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
				});

			std::vector<std::filesystem::path> audioCandidates;
			const std::filesystem::path resultDir = std::filesystem::path("convert") / "result";
			audioCandidates.push_back(resultDir / uniqueid);
			if (lowerFormat == "flac") {
				audioCandidates.push_back(resultDir / (uniqueid + ".flac"));
				audioCandidates.push_back(resultDir / (uniqueid + ".mp3"));
			}
			else {
				audioCandidates.push_back(resultDir / (uniqueid + ".mp3"));
				audioCandidates.push_back(resultDir / (uniqueid + ".flac"));
			}

			std::string removeError;
			for (const auto& candidate : audioCandidates) {
				std::string currentError;
				if (!tryRemoveFileWithRetry(candidate, currentError)) {
					removeError = currentError;
					break;
				}
			}

			if (removeError.empty() && !pictureHash.empty()) {
				std::string coverRemoveError;
				const std::filesystem::path coverPath = std::filesystem::path("convert") / "cover_images" / pictureHash;
				if (!tryRemoveFileWithRetry(coverPath, coverRemoveError)) {
					removeError = coverRemoveError;
				}
			}

			if (!removeError.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete converted files. Error: " + removeError;
				res.SendJson(Json::FastWriter().write(responseJson), 500);
				return;
			}

			if (!nConverter.removeFinishedDatabase(uniqueid)) {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete converted record: " + uniqueid + ".";
				res.SendJson(Json::FastWriter().write(responseJson), 500);
				return;
			}

			if (!nConverter.removePendingDatabase(uniqueid)) {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete pending record for converted file: " + uniqueid + ".";
				res.SendJson(Json::FastWriter().write(responseJson), 500);
				return;
			}

			responseJson["success"] = true;
			responseJson["message"] = "Converted file deleted successfully.";
			res.SendJson(Json::FastWriter().write(responseJson));
			};
		auto handleConvertedDownload = [&nConverter](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string uniqueid = requestJson["uniqueid"].asString();
			Json::Value responseJson;
			if (uniqueid.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid uniqueid.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			std::string format;
          std::string rawName;
			for (const auto& record : nConverter.getConverted()) {
				auto itUnique = record.find("file_name");
				auto itFormat = record.find("file_format");
             auto itRawName = record.find("file_raw_name");
				if (itUnique != record.end() && itFormat != record.end() && itUnique->second == uniqueid) {
					format = itFormat->second;
                  if (itRawName != record.end()) rawName = itRawName->second;
					break;
				}
			}

			std::string lowerFormat = format;
			std::transform(lowerFormat.begin(), lowerFormat.end(), lowerFormat.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
				});

			std::vector<std::filesystem::path> candidates;
			const std::filesystem::path resultDir = std::filesystem::path("convert") / "result";
			candidates.push_back(resultDir / uniqueid);
			if (lowerFormat == "flac") {
				candidates.push_back(resultDir / (uniqueid + ".flac"));
				candidates.push_back(resultDir / (uniqueid + ".mp3"));
			}
			else {
				candidates.push_back(resultDir / (uniqueid + ".mp3"));
				candidates.push_back(resultDir / (uniqueid + ".flac"));
			}

			std::filesystem::path convertedFilePath;
			for (const auto& candidate : candidates) {
				if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
					convertedFilePath = candidate;
					break;
				}
			}

			if (convertedFilePath.empty()) {
				responseJson["success"] = false;
                responseJson["message"] = "Converted file not found.";
				res.SendJson(Json::FastWriter().write(responseJson), 404);
				return;
			}

			std::string contentType = "application/octet-stream";
            std::string finalExt = ".mp3";
			if (lowerFormat == "flac") {
				finalExt = ".flac";
				contentType = "audio/flac";
			}
			else {
				finalExt = ".mp3";
				contentType = "audio/mpeg";
			}

			auto endsWithIgnoreCase = [](const std::string& text, const std::string& suffix) {
				if (suffix.size() > text.size()) return false;
				auto itText = text.end() - static_cast<std::ptrdiff_t>(suffix.size());
				for (size_t i = 0; i < suffix.size(); ++i) {
					if (std::tolower(static_cast<unsigned char>(*(itText + static_cast<std::ptrdiff_t>(i)))) !=
						std::tolower(static_cast<unsigned char>(suffix[i]))) {
						return false;
					}
				}
				return true;
			};

			if (rawName.empty()) rawName = uniqueid;
			std::string downloadName = rawName;
			if (!endsWithIgnoreCase(downloadName, finalExt)) {
				downloadName += finalExt;
			}

          res.SendFileDownload(convertedFilePath.string(), downloadName, contentType);
			};
		auto handleConvertedCover = [&nConverter](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson, requestJson = ReadJsonFromString(req.body);
			const std::string uniqueid = requestJson["uniqueid"].asString();
			if (uniqueid.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid uniqueid.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			std::string pictureHash;
			for (const auto& record : nConverter.getConverted()) {
				auto itUnique = record.find("file_name");
				auto itPicture = record.find("picture_hash");
				if (itUnique != record.end() && itPicture != record.end() && itUnique->second == uniqueid) {
					pictureHash = itPicture->second;
					break;
				}
			}

			if (pictureHash.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "No cover found for uniqueid: " + uniqueid;
				res.SendJson(Json::FastWriter().write(responseJson), 404);
				return;
			}

			const std::filesystem::path filepath = std::filesystem::path("convert") / "cover_images" / pictureHash;
			if (!std::filesystem::exists(filepath) || !std::filesystem::is_regular_file(filepath)) {
				responseJson["success"] = false;
				responseJson["message"] = "Cover file not found.";
				res.SendJson(Json::FastWriter().write(responseJson), 404);
				return;
			}

			std::ifstream imageFile(filepath, std::ios::binary);
			if (!imageFile.is_open()) {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to open cover file.";
				res.SendJson(Json::FastWriter().write(responseJson), 500);
				return;
			}

			const std::string imageData((std::istreambuf_iterator<char>(imageFile)), std::istreambuf_iterator<char>());
			if (imageData.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "Cover file is empty.";
				res.SendJson(Json::FastWriter().write(responseJson), 500);
				return;
			}

			const bool isPng = imageData.size() >= 8 &&
				static_cast<unsigned char>(imageData[0]) == 0x89 &&
				imageData[1] == 'P' && imageData[2] == 'N' && imageData[3] == 'G' &&
				static_cast<unsigned char>(imageData[4]) == 0x0D &&
				static_cast<unsigned char>(imageData[5]) == 0x0A &&
				static_cast<unsigned char>(imageData[6]) == 0x1A &&
				static_cast<unsigned char>(imageData[7]) == 0x0A;

			const std::string mimeType = isPng ? "image/png" : "image/jpeg";
			responseJson["success"] = true;
			responseJson["data"]["filepath"] = filepath.generic_string();
			responseJson["data"]["image"] = "data:" + mimeType + ";base64," + Base64::Encode(imageData);
			res.SendJson(Json::FastWriter().write(responseJson));

			};
		auto handleConvertedPlay = [&nConverter](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson, requestJson = ReadJsonFromString(req.body);
			const std::string uniqueid = requestJson["uniqueid"].asString();
			if (uniqueid.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid uniqueid.";
				res.SendJson(Json::FastWriter().write(responseJson), 400);
				return;
			}

			std::string format;
			for (const auto& record : nConverter.getConverted()) {
				auto itUnique = record.find("file_name");
				auto itFormat = record.find("file_format");
				if (itUnique != record.end() && itFormat != record.end() && itUnique->second == uniqueid) {
					format = itFormat->second;
					break;
				}
			}

			if (format.empty()) {
				responseJson["success"] = false;
				responseJson["message"] = "No converted file found for uniqueid: " + uniqueid;
				res.SendJson(Json::FastWriter().write(responseJson), 404);
				return;
			}

			const std::filesystem::path filepath = std::filesystem::path("convert") / "result" / (uniqueid);
			if (!std::filesystem::exists(filepath) || !std::filesystem::is_regular_file(filepath)) {
				responseJson["success"] = false;
				responseJson["message"] = "Audio file not found.";
				res.SendJson(Json::FastWriter().write(responseJson), 404);
				return;
			}

			responseJson["success"] = true;
			responseJson["data"]["filepath"] = filepath.generic_string();
			std::string lowerFormat = format;
			std::transform(lowerFormat.begin(), lowerFormat.end(), lowerFormat.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
				});
			responseJson["data"]["mime"] = (lowerFormat == "flac") ? "audio/flac" : "audio/mpeg";
			res.SendJson(Json::FastWriter().write(responseJson));
			};

		server.On("/reservation", [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) { //http://localhost:8027/reservation
			res.SendFile("./html/reservation.html", "text/html");
			});
		server.On("/api/reservation/tasks/add", handleReservationTaskAdd);
		server.On("/api/reservation/tasks/delete", handleReservationTaskDelete);
		server.On("/api/reservation/tasks/refresh", handleReservationTaskRefresh);
		server.On("/api/reservation/tasks/autopay/status", handleReservationAutoPayStatus);
		server.On("/api/reservation/tasks/autopay/change", handleReservationAutoPayChange);
		server.On("/api/reservation/pending/pay", handleReservationPendingPay);
		server.On("/api/reservation/pending/delete", handleReservationPendingDelete);

		server.On("/convert", [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) { //http://localhost:8027/convert
			res.SendFile("./html/convert.html", "text/html");
			});
		server.On("/api/convert/upload", handleConvertUpload);
		server.On("/api/convert/pending/refresh", handleConvertRefresh);
		server.On("/api/convert/pending/delete", handleConverterPendingDelete);
		server.On("/api/convert/pending/convert", handleConvert);
		server.On("/api/convert/file/refresh", handleConvertedRefresh);
		server.On("/api/convert/file/download", handleConvertedDownload);
		server.On("/api/convert/file/delete", handleConvertedDelete);
		server.On("/api/convert/file/cover", handleConvertedCover);
		server.On("/api/convert/play", handleConvertedPlay);

		server.Start();
	}
	catch (const std::exception& ex) {
		Logger logger;
		logger.SetLogFile("server.log");
		logger.EnableConsole(true);
		Logger::SetCurrentThreadName("Thread-0");
		logger.Error(std::string("Server error: ") + ex.what());
		return 1;
	}

	return 0;
}
