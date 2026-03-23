#ifndef _CONVERTER_H_
#define _CONVERTER_H_

#include "databaseConnection.h"
#include "base64.h"
#include "Logger.h"
#include "sha256.h"
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tfile.h>

class NeteaseConverter {
private:
	std::map<std::string, std::string> NCMPendingConvertFiles;
	std::mutex NCMFilesMutex;
	DatabaseConnection& dbConnection_;
	Logger& logger_;

	static std::string toDbSafeText(const std::string& text) {
       if (text.rfind("b64:", 0) == 0) {
			return text;
		}
		return "b64:" + Base64::Encode(text);
	}

	static std::string fromDbSafeText(const std::string& text) {
		if (text.rfind("b64:", 0) == 0) {
			std::string decoded;
			if (Base64::Decode(text.substr(4), decoded).empty()) {
				return decoded;
			}
		}
		return text;
	}
public:
	NeteaseConverter(DatabaseConnection& dbConnection, Logger& logger) : dbConnection_(dbConnection), logger_(logger) {}

	bool exportCover(int format, const std::string& filename, const std::string& outputPath, std::string& pictureHash) {
		TagLib::ByteVector data;
		switch (format) {
		case 1:
		{
			TagLib::FLAC::File file(filename.c_str());
			if (!file.isValid()) {
				logger_.Error("Failed to open FLAC file: " + filename);
				return false;
			}

			auto pictures = file.pictureList();
			if (pictures.isEmpty()) {
				logger_.Error("FLAC have no cover: " + filename);
				return false;
			}

			auto* picture = pictures.front();
			if (picture == nullptr) {
				logger_.Error("Can't get FLAC picture: " + filename);
				return false;
			}
			data = picture->data();

			if (data.isEmpty()) {
				logger_.Error("FLAC picture data is empty: " + filename);
				return false;
			}
		}
		break;
		case 0:
		{
			TagLib::MPEG::File file(filename.c_str());
			if (!file.isValid()) {
				logger_.Error("Failed to open MP3 file: " + filename);
				return false;
			}

			TagLib::ID3v2::Tag* id3v2Tag = file.ID3v2Tag();
			if (!id3v2Tag) {
				logger_.Error("MP3 file has no ID3v2 tag: " + filename);
				return false;
			}

			TagLib::ID3v2::FrameList frames = id3v2Tag->frameList("APIC");
			if (frames.isEmpty()) {
				logger_.Error("MP3 file has no cover: " + filename);
				return false;
			}

			TagLib::ID3v2::AttachedPictureFrame* picFrame =
				dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());

			if (!picFrame) {
				logger_.Error("Can't get MP3 picture frame: " + filename);
				return false;
			}

			data = picFrame->picture();

			if (data.isEmpty()) {
				logger_.Error("MP3 picture data is empty: " + filename);
				return false;
			}
		}
		break;
		default:
			logger_.Error("Unsupported file format.");
			return false;
		}

		pictureHash = Sha256::Hex(std::string(data.data(), data.size()));
		std::string dir = outputPath + "/" + pictureHash;

		std::ofstream outFile(dir, std::ios::binary);
		if (!outFile.is_open()) {
			logger_.Error("Failed to create output file: " + dir);
			return false;
		}

		outFile.write(data.data(), data.size());
		if (!outFile) {
			logger_.Error("Failed to write cover file: " + dir);
			return false;
		}
		outFile.close();

		logger_.Info("Cover export completed, filename=" + dir + ", bytes written=" + std::to_string(data.size()));

		return true;
	}

	void initialize() {
		auto pendingMap = getConverterPending();
		std::scoped_lock lock(NCMFilesMutex);
		NCMPendingConvertFiles.clear();
		for (const auto& record : pendingMap) {
			auto itHash = record.find("unique_id");
			auto itName = record.find("file_name");
			if (itHash != record.end() && itName != record.end())
                NCMPendingConvertFiles[itHash->second] = fromDbSafeText(itName->second);
		}
	}
	
	void addPendingMap(std::string uniqueid, std::string filename) {
		std::scoped_lock lock(NCMFilesMutex);
		NCMPendingConvertFiles[uniqueid] = filename;
	}

	void removePendingMap(std::string uniqueid) {
		std::scoped_lock lock(NCMFilesMutex);
		NCMPendingConvertFiles.erase(uniqueid);
	}

	std::string getPendingFilename(std::string uniqueid) {
		std::scoped_lock lock(NCMFilesMutex);
		return NCMPendingConvertFiles[uniqueid];
	}

	bool addPendingDatabase(std::string name, std::string hash, size_t size, std::string username) {
		std::map<std::string, std::string> writeVal;
		writeVal["unique_id"] = hash;
		writeVal["user_id"] = username;
		writeVal["file_name"] = Base64::Encode(name);
		writeVal["file_size"] = std::to_string(size);
		int64_t newId = dbConnection_.insert("converter_pending", writeVal);
		logger_.Info("Insert database [converter_pending]: " + std::to_string(newId));
		return newId != -1;
	}
	bool removePendingDatabase(std::string hash) {
		int deleted = dbConnection_.remove("converter_pending", "unique_id = ?", { hash });
		logger_.Info("Delete database [converter_pending]: " + std::to_string(deleted) + " rows affected, unique_id = " + hash);
		return deleted != -1;
	}
	bool searchPendingExists(std::string hash) {
		return dbConnection_.searchExists("converter_pending", "unique_id", hash);
	}
	bool searchPendingExistedStatus(std::string hash, std::string status) {
		return dbConnection_.searchExistsStatus("converter_pending", "unique_id", hash, "status", status);
	}
	bool changePendingStatus(std::string hash, std::string newStatus) {
		int updated = dbConnection_.update("converter_pending", { {"status", newStatus} }, "unique_id = ?", { hash });
		logger_.Info("Update database [converter_pending]: " + std::to_string(updated) + " rows affected, unique_id = " + hash + ", new status = " + newStatus);
		return updated != -1;
	}
	std::vector<std::map<std::string, std::string>> getConverterPending() {
		auto records = dbConnection_.fetchAll("converter_pending");
		return records;
	}
	std::vector<std::map<std::string, std::string>> getConverted() {
		auto records = dbConnection_.fetchAll("converter_finished");
		return records;
	}
	bool addFinishedDatabase(std::string raw_name, std::string name, int format, std::string pictureHash, size_t fileSize, std::string username) {
		std::map<std::string, std::string> writeVal;
		writeVal["user_id"] = username;
		writeVal["file_raw_name"] = Base64::Encode(raw_name);
		writeVal["file_name"] = name;
		writeVal["picture_hash"] = pictureHash;
		writeVal["file_size"] = std::to_string(fileSize);
		writeVal["file_format"] = format ? "FLAC" : "MP3";
		int64_t newId = dbConnection_.insert("converter_finished", writeVal);
		logger_.Info("Insert database [converter_finished]: " + std::to_string(newId));
		return newId != -1;
	}
	bool removeFinishedDatabase(std::string hash) {
		int deleted = dbConnection_.remove("converter_finished", "file_name = ?", { hash });
		logger_.Info("Delete database [converter_finished]: " + std::to_string(deleted) + " rows affected, file_name = " + hash);
		return deleted != -1;
	}
	bool addConfig(std::string config, std::string value) {
		std::map<std::string, std::string> writeVal;
		writeVal["config_key"] = config;
		writeVal["config_value"] = value;
		int64_t newId = dbConnection_.insert("config", writeVal);
		logger_.Info("Insert database [config]: " + std::to_string(newId));
		return newId != -1;
	}
	std::string getConfig(std::string config) {
		auto record = dbConnection_.fetchOne("config", "config_key", config);
		if (record.empty()) return "";
		return record.at("config_value");
	}
};

#endif