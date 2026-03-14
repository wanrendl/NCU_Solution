#include "HTTPBackend.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#pragma comment(lib, "ws2_32.lib")

HttpServer::HttpResponse::HttpResponse(SOCKET clientSocket) : clientSocket_(clientSocket) {}

const std::unordered_map<int, std::string>& HttpServer::HttpResponse::GetAllHttpStatusDescriptions() {
	static const std::unordered_map<int, std::string> statusMap = {
		{100, "Continue"}, {101, "Switching Protocols"}, {102, "Processing"}, {103, "Early Hints"},
		{200, "OK"}, {201, "Created"}, {202, "Accepted"}, {203, "Non-Authoritative Information"},
		{204, "No Content"}, {205, "Reset Content"}, {206, "Partial Content"}, {207, "Multi-Status"},
		{208, "Already Reported"}, {226, "IM Used"},
		{300, "Multiple Choices"}, {301, "Moved Permanently"}, {302, "Found"}, {303, "See Other"},
		{304, "Not Modified"}, {305, "Use Proxy"}, {307, "Temporary Redirect"}, {308, "Permanent Redirect"},
		{400, "Bad Request"}, {401, "Unauthorized"}, {402, "Payment Required"}, {403, "Forbidden"},
		{404, "Not Found"}, {405, "Method Not Allowed"}, {406, "Not Acceptable"},
		{407, "Proxy Authentication Required"}, {408, "Request Timeout"}, {409, "Conflict"},
		{410, "Gone"}, {411, "Length Required"}, {412, "Precondition Failed"},
		{413, "Payload Too Large"}, {414, "URI Too Long"}, {415, "Unsupported Media Type"},
		{416, "Range Not Satisfiable"}, {417, "Expectation Failed"}, {418, "I'm a teapot"},
		{421, "Misdirected Request"}, {422, "Unprocessable Entity"}, {423, "Locked"},
		{424, "Failed Dependency"}, {425, "Too Early"}, {426, "Upgrade Required"},
		{428, "Precondition Required"}, {429, "Too Many Requests"},
		{431, "Request Header Fields Too Large"}, {451, "Unavailable For Legal Reasons"},
		{500, "Internal Server Error"}, {501, "Not Implemented"}, {502, "Bad Gateway"},
		{503, "Service Unavailable"}, {504, "Gateway Timeout"}, {505, "HTTP Version Not Supported"},
		{506, "Variant Also Negotiates"}, {507, "Insufficient Storage"}, {508, "Loop Detected"},
		{510, "Not Extended"}, {511, "Network Authentication Required"}
	};
	return statusMap;
}

void HttpServer::HttpResponse::Send(int statusCode, std::string_view contentType, std::string_view body) {
	const std::string response =
		"HTTP/1.1 " + std::to_string(statusCode) + " " + GetStatusText(statusCode) + "\r\n"
		"Content-Type: " + std::string(contentType) + "\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"Connection: close\r\n\r\n" +
		std::string(body);

	SendAll(response.data(), response.size());
	sent_ = true;
}

void HttpServer::HttpResponse::SendHtml(std::string_view html, int statusCode) {
	Send(statusCode, "text/html; charset=utf-8", html);
}

void HttpServer::HttpResponse::SendText(std::string_view text, int statusCode) {
	Send(statusCode, "text/plain; charset=utf-8", text);
}

void HttpServer::HttpResponse::SendJson(std::string_view json, int statusCode) {
	Send(statusCode, "application/json; charset=utf-8", json);
}

void HttpServer::HttpResponse::SendStatusDescription(int statusCode) {
	SendText(std::to_string(statusCode) + " " + GetStatusText(statusCode), statusCode);
}

void HttpServer::HttpResponse::SendFile(const std::string& filePath, const std::string& contentType) {
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		SendText("File Not Found", 404);
		return;
	}

	const std::string body{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	const auto type = contentType.empty() ? GuessContentType(filePath) : contentType;
	Send(200, type, body);
}

bool HttpServer::HttpResponse::HasSent() const noexcept {
	return sent_;
}

void HttpServer::HttpResponse::SendAll(const char* data, size_t length) {
	size_t total = 0;
	while (total < length) {
		const int sentBytes = send(clientSocket_, data + total, static_cast<int>(length - total), 0);
		if (sentBytes <= 0) {
			break;
		}
		total += static_cast<size_t>(sentBytes);
	}
}

std::string HttpServer::HttpResponse::GuessContentType(const std::string& filePath) {
	static constexpr std::array<std::pair<std::string_view, std::string_view>, 9> contentTypes = {
		std::pair{ ".html", "text/html; charset=utf-8" },
		std::pair{ ".htm", "text/html; charset=utf-8" },
		std::pair{ ".txt", "text/plain; charset=utf-8" },
		std::pair{ ".json", "application/json; charset=utf-8" },
		std::pair{ ".css", "text/css; charset=utf-8" },
		std::pair{ ".js", "application/javascript; charset=utf-8" },
		std::pair{ ".png", "image/png" },
		std::pair{ ".jpg", "image/jpeg" },
		std::pair{ ".jpeg", "image/jpeg" }
	};

	const std::string extension = std::filesystem::path(filePath).extension().string();
	for (const auto& [ext, mime] : contentTypes) {
		if (extension == ext) {
			return std::string(mime);
		}
	}

	if (extension == ".gif") return "image/gif";
	if (extension == ".svg") return "image/svg+xml";
	return "application/octet-stream";
}

std::string HttpServer::HttpResponse::GetStatusText(int statusCode) {
	const auto& statusMap = GetAllHttpStatusDescriptions();
	if (const auto it = statusMap.find(statusCode); it != statusMap.end()) {
		return it->second;
	}
	return "Unknown Status";
}

HttpServer::HttpServer(unsigned short port, Logger& logger) : port_(port), logger_(logger) {}

HttpServer::~HttpServer() {
	Stop();
}

void HttpServer::SetHtmlContent(std::string_view htmlContent) {
	std::scoped_lock lock(stateMutex_);
	htmlContent_ = std::string(htmlContent);
}

void HttpServer::On(std::string routePath, RouteHandler handler) {
	std::scoped_lock lock(stateMutex_);
	routes_[std::move(routePath)] = std::move(handler);
}

void HttpServer::Start() {
	if (running_) {
		throw std::runtime_error("Server is already running");
	}

	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		throw std::runtime_error("WSAStartup failed");
	}

	listenSocket_ = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket_ == INVALID_SOCKET) {
		WSACleanup();
		throw std::runtime_error("socket() failed");
	}

	int off = 0;
	if (setsockopt(listenSocket_, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off)) == SOCKET_ERROR) {
		closesocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET;
		WSACleanup();
		throw std::runtime_error("setsockopt(IPV6_V6ONLY) failed");
	}

	sockaddr_in6 serverAddr{};
	serverAddr.sin6_family = AF_INET6;
	serverAddr.sin6_port = htons(port_);
	serverAddr.sin6_addr = in6addr_any;

	if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
		closesocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET;
		WSACleanup();
		throw std::runtime_error("bind() failed");
	}

	if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
		closesocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET;
		WSACleanup();
		throw std::runtime_error("listen() failed");
	}

	running_ = true;
	stopping_ = false;
	logger_.Info("HTTP server listening on port " + std::to_string(port_) + " (IPv4 + IPv6)");
	StartWorkers();

	while (running_) {
		sockaddr_storage clientAddr{};
		int clientLen = sizeof(clientAddr);
		const SOCKET clientSocket = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
		if (clientSocket == INVALID_SOCKET) {
			if (running_) {
				logger_.Error("accept() failed: " + std::to_string(WSAGetLastError()));
			}
			continue;
		}

		{
			std::scoped_lock lock(queueMutex_);
			clientQueue_.push(clientSocket);
		}
		queueCv_.notify_one();
	}
}

void HttpServer::Stop() {
	if (!running_) {
		return;
	}

	running_ = false;
	logger_.Info("HTTP server stopping");
	if (listenSocket_ != INVALID_SOCKET) {
		closesocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET;
	}

	{
		std::scoped_lock lock(queueMutex_);
		stopping_ = true;
	}
	queueCv_.notify_all();

	for (auto& worker : workers_) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	workers_.clear();

	{
		std::scoped_lock lock(queueMutex_);
		while (!clientQueue_.empty()) {
			closesocket(clientQueue_.front());
			clientQueue_.pop();
		}
	}

	WSACleanup();
	logger_.Info("HTTP server stopped");
}

void HttpServer::StartWorkers() {
	const auto count = (std::max)(2u, std::thread::hardware_concurrency());
	workers_.reserve(count);
	for (unsigned int i = 0; i < count; ++i) {
		workers_.emplace_back([this]() {
			WorkerLoop();
		});
	}
	logger_.Info("Worker threads started: " + std::to_string(count));
}

void HttpServer::WorkerLoop() {
	for (;;) {
		SOCKET clientSocket = INVALID_SOCKET;
		{
			std::unique_lock<std::mutex> lock(queueMutex_);
			queueCv_.wait(lock, [this]() {
				return stopping_ || !clientQueue_.empty();
			});

			if (stopping_ && clientQueue_.empty()) {
				return;
			}

			clientSocket = clientQueue_.front();
			clientQueue_.pop();
		}

		HandleClient(clientSocket);
		closesocket(clientSocket);
	}
}

std::string HttpServer::Trim(std::string_view text) {
	size_t start = 0;
	while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
		++start;
	}

	size_t end = text.size();
	while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
		--end;
	}

	return std::string(text.substr(start, end - start));
}

HttpServer::HttpRequest HttpServer::ParseRequest(const std::string& rawRequest) {
	HttpRequest request;

	const size_t headerEndPos = rawRequest.find("\r\n\r\n");
	const std::string headerPart = (headerEndPos == std::string::npos) ? rawRequest : rawRequest.substr(0, headerEndPos);
	request.body = (headerEndPos == std::string::npos) ? "" : rawRequest.substr(headerEndPos + 4);

	std::istringstream stream(headerPart);
	std::string requestLine;
	if (!std::getline(stream, requestLine)) {
		return request;
	}

	requestLine = Trim(requestLine);

	{
		std::istringstream lineStream(requestLine);
		lineStream >> request.method;
		lineStream >> request.rawPath;
	}

	if (request.rawPath.empty()) {
		request.path = "/";
	}
	else if (const size_t queryPos = request.rawPath.find('?'); queryPos != std::string::npos) {
		request.path = request.rawPath.substr(0, queryPos);
	}
	else {
		request.path = request.rawPath;
	}

	std::string headerLine;
	while (std::getline(stream, headerLine)) {
		headerLine = Trim(headerLine);
		if (headerLine.empty()) {
			continue;
		}

		if (const size_t colonPos = headerLine.find(':'); colonPos != std::string::npos) {
			const std::string key = Trim(std::string_view(headerLine).substr(0, colonPos));
			const std::string value = Trim(std::string_view(headerLine).substr(colonPos + 1));
			request.headers[key] = value;
		}
	}

	return request;
}

bool HttpServer::TryServeLocalFile(const std::string& requestPath, HttpResponse& response) {
	if (requestPath.empty() || requestPath == "/") {
		return false;
	}

	std::string localPath = requestPath;
	if (!localPath.empty() && localPath.front() == '/') {
		localPath.erase(localPath.begin());
	}

	if (localPath.empty() || localPath.find("..") != std::string::npos) {
		return false;
	}

	const std::filesystem::path filePath(localPath);
	if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
		return false;
	}

	response.SendFile(filePath.string());
	return true;
}

void HttpServer::HandleClient(SOCKET clientSocket) {
	std::array<char, 2048> buffer{};
	const int recvLen = recv(clientSocket, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
	if (recvLen <= 0) {
		logger_.Warning("recv() failed or empty request");
		return;
	}

	const std::string rawRequest(buffer.data(), static_cast<size_t>(recvLen));
	const HttpRequest request = ParseRequest(rawRequest);
	HttpResponse response(clientSocket);
	logger_.Info("Request: method=" + request.method + " path=" + request.path);

	RouteHandler handler;
	{
		std::scoped_lock lock(stateMutex_);
		if (const auto it = routes_.find(request.path); it != routes_.end()) {
			handler = it->second;
		}
	}

	if (handler) {
		try {
			handler(request, response);
		}
		catch (const std::exception& ex) {
			if (!response.HasSent()) {
				response.SendText("Internal Server Error", 500);
			}
			logger_.Error("Route handler exception: " + std::string(ex.what()));
		}
		catch (...) {
			if (!response.HasSent()) {
				response.SendText("Internal Server Error", 500);
			}
			logger_.Error("Route handler unknown exception");
		}
	}
	else {
		if (!TryServeLocalFile(request.path, response)) {
			response.SendText("Not Found", 404);
			logger_.Warning("Route not found: " + request.path);
		}
	}

	if (!response.HasSent()) {
		response.SendText("No response generated by route handler", 500);
		logger_.Error("No response generated by route handler for path: " + request.path);
	}
}
