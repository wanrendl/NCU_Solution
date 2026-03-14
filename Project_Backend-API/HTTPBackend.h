#ifndef _HTTP_BACKEND_H_
#define _HTTP_BACKEND_H_

#include "Logger.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

class HttpServer {
public:
	struct HttpRequest {
		std::string method;
		std::string path;
		std::string rawPath;
		std::string body;
		std::unordered_map<std::string, std::string> headers;
	};

	class HttpResponse {
	public:
		explicit HttpResponse(SOCKET clientSocket);

		[[nodiscard]] static const std::unordered_map<int, std::string>& GetAllHttpStatusDescriptions();

		void Send(int statusCode, std::string_view contentType, std::string_view body);
		void SendHtml(std::string_view html, int statusCode = 200);
		void SendText(std::string_view text, int statusCode = 200);
		void SendJson(std::string_view json, int statusCode = 200);
		void SendStatusDescription(int statusCode);
		void SendFile(const std::string& filePath, const std::string& contentType = "");
		[[nodiscard]] bool HasSent() const noexcept;

	private:
		void SendAll(const char* data, size_t length);
		[[nodiscard]] static std::string GuessContentType(const std::string& filePath);
		[[nodiscard]] static std::string GetStatusText(int statusCode);

	private:
		SOCKET clientSocket_{ INVALID_SOCKET };
		bool sent_{ false };
	};

	using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

	explicit HttpServer(unsigned short port, Logger& logger);
	~HttpServer();

	void SetHtmlContent(std::string_view htmlContent);
	void On(std::string routePath, RouteHandler handler);
	void Start();
	void Stop();

private:
	void StartWorkers();
	void WorkerLoop();
	[[nodiscard]] static std::string Trim(std::string_view text);
	[[nodiscard]] static HttpRequest ParseRequest(const std::string& rawRequest);
	[[nodiscard]] static bool TryServeLocalFile(const std::string& requestPath, HttpResponse& response);
	void HandleClient(SOCKET clientSocket);

private:
	unsigned short port_;
	std::string htmlContent_;
	std::unordered_map<std::string, RouteHandler> routes_;
	std::mutex stateMutex_;
	std::vector<std::thread> workers_;
	std::queue<SOCKET> clientQueue_;
	std::mutex queueMutex_;
	std::condition_variable queueCv_;
	bool stopping_{ false };
	std::atomic<bool> running_{ false };
	SOCKET listenSocket_{ INVALID_SOCKET };
	Logger& logger_;
};

#endif // !_HTTP_BACKEND_H_