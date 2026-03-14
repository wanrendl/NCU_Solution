#include <iostream>
#include "HTTPBackend.h"
#include "NCUReservation.h"
#include "json.h"

int main() {
	try {
		Logger logger;
		logger.SetLogFile("server.log");
		logger.EnableConsole(true);
		Logger::SetCurrentThreadName("Thread-0");

		ReservationManager reservation(logger);
		reservation.begin();

		HttpServer server(8080, logger);

		server.On("/reservation", [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			res.SendFile("index.html", "text/html");
		});

		server.On("/api/tasks/add", [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
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
		});

		server.On("/api/tasks/refresh", [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value responseJson, requestJson = ReadJsonFromString(req.body);
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
				else if (date - 2 > CurrentTime().GetFormattedTimeDate()) {
					responseJson["data"]["overdate"] = true;
				}
				else {
					Json::Value table;
					if (reservation.getReservationTable(date.Print(), table)) {
						responseJson["data"]["overdate"] = false;
						responseJson["data"]["table"] = table;
					}
					else {
						responseJson["success"] = false;
						responseJson["message"] = "Failed to get reservation table.";
					}
				}
			}
			else {
				responseJson["success"] = false;
				responseJson["message"] = "Invalid type.";
			}
			res.SendJson(Json::FastWriter().write(responseJson));
		});

		server.On("/api/tasks/delete", [&reservation](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
			Json::Value requestJson = ReadJsonFromString(req.body);
			std::string uniqueid = requestJson["uniqueid"].asString();

			Json::Value responseJson;

			if (reservation.deleteReservation(uniqueid)) {
				responseJson["success"] = true;
				responseJson["message"] = "Reservation: " + uniqueid + " deleted successfully.";
			} else {
				responseJson["success"] = false;
				responseJson["message"] = "Failed to delete reservation: " + uniqueid + ".";
			}

			res.SendJson(Json::FastWriter().write(responseJson));
		});

		//server.On("/api/hello", [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
		//	res.SendJson("{\"message\":\"hello\",\"path\":\"" + req.path + "\"}");
		//});

		//server.On("/api/echo", [](const HttpServer::HttpRequest& req, HttpServer::HttpResponse& res) {
		//	res.SendText("method=" + req.method + "\npath=" + req.rawPath + "\nbody=" + req.body);
		//});

		//server.On("/api/file", [](const HttpServer::HttpRequest&, HttpServer::HttpResponse& res) {
		//	res.SendFile("index.html");
		//});

		//server.On("/api/http-codes", [](const HttpServer::HttpRequest&, HttpServer::HttpResponse& res) {
		//	std::ostringstream out;
		//	for (const auto& [code, desc] : HttpServer::HttpResponse::GetAllHttpStatusDescriptions()) {
		//		out << code << " " << desc << "\n";
		//	}
		//	res.SendText(out.str());
		//});

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
