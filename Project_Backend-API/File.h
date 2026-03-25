#ifndef _FILE_H_
#define _FILE_H_

#include "databaseConnection.h"
#include "Logger.h"
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

class FileDeleteManager {
private:
	std::mutex mutex_;
	std::condition_variable cv_;
	std::map<std::string, size_t> activeAccess_;

	void release(const std::string& fileHash) {
		std::scoped_lock lock(mutex_);
		auto it = activeAccess_.find(fileHash);
		if (it == activeAccess_.end()) {
			return;
		}
		if (it->second > 1) {
			it->second -= 1;
		}
		else {
			activeAccess_.erase(it);
		}
		cv_.notify_all();
	}

public:
	class AccessGuard {
	private:
		FileDeleteManager* manager_;
		std::string fileHash_;

	public:
		AccessGuard(FileDeleteManager* manager, std::string fileHash)
			: manager_(manager), fileHash_(std::move(fileHash)) {
		}

		AccessGuard(const AccessGuard&) = delete;
		AccessGuard& operator=(const AccessGuard&) = delete;

		AccessGuard(AccessGuard&& other) noexcept
			: manager_(other.manager_), fileHash_(std::move(other.fileHash_)) {
			other.manager_ = nullptr;
		}

		AccessGuard& operator=(AccessGuard&& other) noexcept {
			if (this == &other) {
				return *this;
			}
			if (manager_ != nullptr) {
				manager_->release(fileHash_);
			}
			manager_ = other.manager_;
			fileHash_ = std::move(other.fileHash_);
			other.manager_ = nullptr;
			return *this;
		}

		~AccessGuard() {
			if (manager_ != nullptr) {
				manager_->release(fileHash_);
			}
		}
	};

	AccessGuard acquire(const std::string& fileHash) {
		std::scoped_lock lock(mutex_);
		activeAccess_[fileHash] += 1;
		return AccessGuard(this, fileHash);
	}

	bool waitForIdle(const std::string& fileHash, std::chrono::milliseconds timeout) {
		std::unique_lock<std::mutex> lock(mutex_);
		return cv_.wait_for(lock, timeout, [&]() {
			auto it = activeAccess_.find(fileHash);
			return it == activeAccess_.end() || it->second == 0;
		});
	}

	bool deleteFileWithRetry(const std::filesystem::path& filePath, const std::string& fileHash, Logger& logger, int retryTimes = 8, int waitMs = 120) {
		if (!waitForIdle(fileHash, std::chrono::milliseconds(5000))) {
			logger.Error("Failed to delete file: " + filePath.string() + ". File is still in use.");
			return false;
		}

		std::string lastError;
		for (int i = 0; i < retryTimes; ++i) {
			std::error_code ec;
			if (std::filesystem::remove(filePath, ec)) {
				return true;
			}
			if (!ec && !std::filesystem::exists(filePath)) {
				return true;
			}
			lastError = ec.message();
			std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
		}

		logger.Error("Failed to delete file: " + filePath.string() + ". Error: " + lastError);
		return false;
	}
};

class FileManager {
private:
	DatabaseConnection& dbConn_;
	Logger& logger_;
    FileDeleteManager deleteManager_;
private:
	std::map<std::string, std::string> mimeTypeMap{
		// 文本
		{"txt", "text/plain"},
		{"html", "text/html"},
		{"htm", "text/html"},
		{"css", "text/css"},
		{"js", "application/javascript"},
		{"json", "application/json"},
		{"xml", "application/xml"},
		{"csv", "text/csv"},

		// 图片
		{"jpg", "image/jpeg"},
		{"jpeg", "image/jpeg"},
		{"png", "image/png"},
		{"gif", "image/gif"},
		{"bmp", "image/bmp"},
		{"webp", "image/webp"},
		{"svg", "image/svg+xml"},
		{"ico", "image/x-icon"},

		// 音频
		{"mp3", "audio/mpeg"},
		{"wav", "audio/wav"},
		{"ogg", "audio/ogg"},
		{"flac", "audio/flac"},
		{"aac", "audio/aac"},
		{"m4a", "audio/mp4"},

		// 视频
		{"mp4", "video/mp4"},
		{"avi", "video/x-msvideo"},
		{"mov", "video/quicktime"},
		{"wmv", "video/x-ms-wmv"},
		{"flv", "video/x-flv"},
		{"mkv", "video/x-matroska"},
		{"webm", "video/webm"},

		// 文档
		{"pdf", "application/pdf"},
		{"doc", "application/msword"},
		{"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
		{"xls", "application/vnd.ms-excel"},
		{"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
		{"ppt", "application/vnd.ms-powerpoint"},
		{"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
		{"odt", "application/vnd.oasis.opendocument.text"},
		{"ods", "application/vnd.oasis.opendocument.spreadsheet"},
		{"odp", "application/vnd.oasis.opendocument.presentation"},

		// 压缩包
		{"zip", "application/zip"},
		{"rar", "application/vnd.rar"},
		{"7z", "application/x-7z-compressed"},
		{"tar", "application/x-tar"},
		{"gz", "application/gzip"},
		{"bz2", "application/x-bzip2"},

		// 应用程序
		{"exe", "application/vnd.microsoft.portable-executable"},
		{"msi", "application/x-msi"},
		{"bin", "application/octet-stream"},

		// 其他常见
		{"rtf", "application/rtf"},
		{"md", "text/markdown"},
		{"sh", "application/x-sh"},
		{"py", "text/x-python"},
		{"java", "text/x-java-source"},
		{"c", "text/x-csrc"},
		{"cpp", "text/x-c++src"},
		{"h", "text/x-chdr"}
	};
public:
	FileManager(DatabaseConnection& dbConn, Logger& logger) : dbConn_(dbConn), logger_(logger) {}
	bool dbFileExists(const std::string& fileHash) {
		return dbConn_.searchExists("file_list", "file_hash", fileHash);
	}
	bool dbGetUserFilelist(std::string username, std::vector<std::map<std::string, std::string>>& userFileList) {
		//if (!dbConn_.fetchSpecific("user_files", "user_id", username, userFileList)) {
		//	logger_.Error("Failed to fetch user file list from database for user: " + username);
		//	return false;
		//}
		if (!dbConn_.fetchFiles(username, userFileList)) {
			logger_.Error("Failed to fetch user file list from database for user: " + username);
			return false;
		}
		return true;
	}
	bool dbGetPublicFileList(std::vector<std::map<std::string, std::string>>& userFileList) {
     std::stringstream ssSQL;
		ssSQL << "SELECT "
			<< "user_files.id AS user_file_id, "
			<< "user_files.user_id, "
			<< "user_files.own_name AS file_name, "
			<< "user_files.uploaded_at, "
			<< "file_list.file_hash, "
			<< "file_list.file_size, "
			<< "file_list.mime_type "
			<< "FROM user_files "
			<< "JOIN file_list ON user_files.file_hash = file_list.file_hash "
			<< "WHERE file_list.type = 'PUB' "
			<< "ORDER BY user_files.uploaded_at DESC;";

		auto res = dbConn_.query(ssSQL.str());
		if (!res) {
			logger_.Error("Failed to fetch public file list from database.");
			return false;
		}

		try {
			sql::ResultSetMetaData* meta = res->getMetaData();
			const int colCount = meta->getColumnCount();
			while (res->next()) {
				std::map<std::string, std::string> row;
				for (int i = 1; i <= colCount; ++i) {
					row[meta->getColumnName(i)] = res->getString(i);
				}
				userFileList.push_back(std::move(row));
			}
		}
		catch (const sql::SQLException& e) {
			logger_.Error(std::string("Failed to parse public file list: ") + e.what());
			userFileList.clear();
			return false;
		}

		return true;
	}
  bool dbGetPublicFileInfo(const std::string& fileHash, std::map<std::string, std::string>& fileInfo) {
		std::stringstream ssSQL;
		ssSQL << "SELECT file_hash, file_size, mime_type, file_raw_name "
			<< "FROM file_list "
			<< "WHERE file_hash = '" << dbConn_.escapeString(fileHash) << "' AND type = 'PUB' LIMIT 1";

		auto res = dbConn_.query(ssSQL.str());
		if (!res) {
			logger_.Error("Failed to query public file info.");
			return false;
		}

		try {
			if (!res->next()) {
				return false;
			}
			sql::ResultSetMetaData* meta = res->getMetaData();
			const int colCount = meta->getColumnCount();
			for (int i = 1; i <= colCount; ++i) {
				fileInfo[meta->getColumnName(i)] = res->getString(i);
			}
		}
		catch (const sql::SQLException& e) {
			logger_.Error(std::string("Failed to parse public file info: ") + e.what());
			fileInfo.clear();
			return false;
		}

		return !fileInfo.empty();
	}
	/*
	[
	  {
		"user_file_id": 101,
		"file_name": "项目报告.pdf",
		"uploaded_at": "2025-03-24 10:30:00",
		"file_hash": "a1b2c3d4e5f6...",
		"file_size": 524288,
		"mime_type": "application/pdf"
	  }
	]
	*/
	bool dbAddNewFile(const std::string& fileHash, size_t fileSize, const std::string& mime, const std::string& type, const std::string& raw_filename) {
		std::map<std::string, std::string> fields = {
			{"file_hash", fileHash},
			{"file_size", std::to_string(fileSize)},
			{"mime_type", mime},
			{"type", type},
			{"file_raw_name", raw_filename}
		};
		int64_t newId = dbConn_.insert("file_list", fields);
		if (newId < 0) {
			logger_.Error("Failed to insert file record into database.");
			return false;
		}
		return true;
	}
	bool dbAddNewUserRelation(const std::string& userId, const std::string& fileHash, const std::string& ownNameBase64) {
		std::map<std::string, std::string> fieldsUserFiles = {
			{"user_id", userId},
			{"file_hash", fileHash},
			{"own_name", ownNameBase64}
		};
		int64_t newId = dbConn_.insert("user_files", fieldsUserFiles);
		if (newId < 0) {
			logger_.Error("Failed to insert user file record into database.");
			return false;
		}
		return true;
	}
	bool dbDeleteFile(const std::string& user_id, const std::string& user_file_id, const std::string& fileHash, std::string& info) {
		/*
		DELETE FROM user_files WHERE user_id = [user_id] AND id = [user_file_id];

		//*/
		//sql = "DELETE FROM user_files WHERE user_id = '" + dbConn_.escapeString(user_id) + "' AND id = " + dbConn_.escapeString(user_file_id) + ";";
		//auto res = dbConn_.query(sql);
		std::map<std::string, std::string> condition = {
			{"user_id", user_id},
			{"id", user_file_id}
		};
		if (dbConn_.removeConditions("user_files", condition) == -1) {
			logger_.Error("Failed to delete user file record from database for user_id: " + user_id + ", user_file_id: " + user_file_id);
			info = "Failed to delete user file record from database.";
			return false;
		} // 删除用户文件记录成功后，检查是否还有其他用户引用同一文件

     {
			std::string sql = "SELECT COUNT(*) FROM user_files WHERE file_hash = '" + dbConn_.escapeString(fileHash) + "'";
			auto res = dbConn_.query(sql);
			if (!res || !res->next() || res->getInt(1) > 0) {
				logger_.Info("File with hash " + fileHash + " is still referenced by user_files, skipping database deletion.");
				info = "File is still referenced by other users, cannot delete file record from database.";
				return true;// 还有其他用户引用该文件，不能删除 file_list 中的记录
			}// 没有其他用户引用该文件，可以安全删除 file_list 中的记录
		}

		if (dbConn_.remove("file_list", "file_hash = ?", { fileHash }) == -1) {
			logger_.Error("Failed to delete file record from database for file_hash: " + fileHash);
			info = "Failed to delete file record from database.";
			return false;
		}

		if (!lcDeleteFile(fileHash)) {
			info = "Failed to delete file from local storage, but database record has been removed.";
			return false;
		}

		return true;
	}
	bool dbGetFileInfo(const std::string& userid, const std::string& user_fileid, std::map<std::string, std::string>& fileInfo) {
		return dbConn_.fetchSpecificFileInfo(userid, user_fileid, fileInfo);
	}
	bool dbDeletePublicFile(const std::string& fileHash, std::string& info) {
		if (fileHash.empty()) {
			info = "Invalid file hash.";
			return false;
		}

		const int affected = dbConn_.remove("file_list", "file_hash = ? AND type = ?", { fileHash, "PUB" });
		if (affected <= 0) {
			logger_.Error("Failed to delete public file record for file_hash: " + fileHash);
			info = "Failed to delete public file record from database.";
			return false;
		}

		if (!lcDeleteFile(fileHash)) {
			info = "Failed to delete file from local storage, but database record has been removed.";
			return false;
		}

		return true;
	}
 bool dbChangeFileType(const std::string& username, const std::string& fileHash, const std::string& targetType, std::string& info) {
		if (username.empty() || fileHash.empty()) {
			info = "Invalid user or file hash.";
			return false;
		}

		if (targetType != "PRI" && targetType != "PUB") {
			info = "Invalid target type.";
			return false;
		}

		{
			std::stringstream ownerSql;
			ownerSql << "SELECT COUNT(*) FROM user_files WHERE user_id = '"
				<< dbConn_.escapeString(username) << "' AND file_hash = '"
				<< dbConn_.escapeString(fileHash) << "'";
			auto ownerRes = dbConn_.query(ownerSql.str());
			if (!ownerRes || !ownerRes->next() || ownerRes->getInt(1) <= 0) {
				info = "Only uploader can change file visibility.";
				return false;
			}
		}

		std::string currentType;
		{
			std::stringstream typeSql;
			typeSql << "SELECT type FROM file_list WHERE file_hash = '"
				<< dbConn_.escapeString(fileHash) << "' LIMIT 1";
			auto typeRes = dbConn_.query(typeSql.str());
			if (!typeRes || !typeRes->next()) {
				info = "File metadata not found.";
				return false;
			}
			currentType = typeRes->getString(1);
		}

		if (currentType == targetType) {
			info = "File visibility unchanged.";
			return true;
		}

		const int updated = dbConn_.update("file_list", { {"type", targetType} }, "file_hash = ?", { fileHash });
		if (updated <= 0) {
			info = "Failed to update file visibility.";
			return false;
		}

		info = "File visibility updated.";
		return true;
	}
	std::string getMimeType(const std::string& extension) {
		auto it = mimeTypeMap.find(extension);
		if (it != mimeTypeMap.end()) return it->second;
		return "application/octet-stream";
	}

	bool lcAddFile(const std::string& fileHash, const std::string& fileContent) {
		std::filesystem::path filePath = std::filesystem::path("files") / fileHash;
		std::ofstream outFile(filePath, std::ios::binary);
		if (!outFile) {
			logger_.Error("Failed to create file: " + filePath.string());
			return false;
		}
		outFile.write(fileContent.data(), fileContent.size());
		if (!outFile) {
			logger_.Error("Failed to write file: " + filePath.string());
			return false;
		}
		return true;
	}
	bool lcDeleteFile(const std::string& fileHash) {
		std::filesystem::path filePath = std::filesystem::path("files") / fileHash;
     return deleteManager_.deleteFileWithRetry(filePath, fileHash, logger_);
	}
  FileDeleteManager::AccessGuard holdFileAccess(const std::string& fileHash) {
		return deleteManager_.acquire(fileHash);
	}
};

#endif // !_FILE_H_