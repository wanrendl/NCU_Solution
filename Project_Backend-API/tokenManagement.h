#ifndef _TOKEN_MANAGEMENT_H_
#define _TOKEN_MANAGEMENT_H_

#include "base64.h"
#include "sha256.h"
#include "databaseConnection.h"
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

class TokenManagement {
private:
	using TimePoint = std::chrono::steady_clock::time_point;
	struct TokenEntry {
		std::string username;
		TimePoint expiresAt;
	};

	std::unordered_map<std::string, TokenEntry> tokenList;
	std::mutex tokenMutex; // Mutex to protect access to the token list
 std::chrono::hours tokenTtl_{ 24 };

	TimePoint getExpiresAt() const {
		return std::chrono::steady_clock::now() + tokenTtl_;
	}

	void clearExpiredTokens() {
		const auto now = std::chrono::steady_clock::now();
		for (auto it = tokenList.begin(); it != tokenList.end();) {
			if (it->second.expiresAt <= now) {
				it = tokenList.erase(it);
			}
			else {
				++it;
			}
		}
	}

public:
   struct TokenDebugInfo {
		std::string token;
		std::string username;
		long long remainingSeconds;
	};

	bool removeToken(const std::string& token) {
		std::lock_guard<std::mutex> lock(tokenMutex);
		clearExpiredTokens();
		return tokenList.erase(token) > 0;
	}

	bool getTokenUsername(const std::string& token, std::string& username) {
		std::lock_guard<std::mutex> lock(tokenMutex);
		clearExpiredTokens();
		auto it = tokenList.find(token);
		if (it == tokenList.end()) {
			return false;
		}
		it->second.expiresAt = getExpiresAt();
		username = it->second.username;
		return true;
	}

	bool checkTokenValidation(const std::string& token) {
		std::lock_guard<std::mutex> lock(tokenMutex);
		clearExpiredTokens();
		auto it = tokenList.find(token);
		if (it == tokenList.end()) {
			return false;
		}
		it->second.expiresAt = getExpiresAt();
		return true;
	}

	bool tokenRefresh(const std::string& token) {
		std::lock_guard<std::mutex> lock(tokenMutex);
		clearExpiredTokens();
		auto it = tokenList.find(token);
		if (it == tokenList.end()) {
			return false;
		}
		it->second.expiresAt = getExpiresAt();
		return true;
	}

	std::string storeNewToken(const std::string& username, const std::string& execution) {
		const std::string newToken = Sha256::Hex(username + execution + std::to_string(std::random_device{}()));
		std::lock_guard<std::mutex> lock(tokenMutex);
	  clearExpiredTokens();
		tokenList[newToken] = TokenEntry{ username, getExpiresAt() };
		return newToken;
	}

	std::vector<TokenDebugInfo> getTokenDebugInfo() {
		std::vector<TokenDebugInfo> infos;
		std::lock_guard<std::mutex> lock(tokenMutex);
		clearExpiredTokens();
		const auto now = std::chrono::steady_clock::now();
		infos.reserve(tokenList.size());
		for (const auto& [token, entry] : tokenList) {
			const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(entry.expiresAt - now).count();
			if (remaining > 0) {
				infos.push_back(TokenDebugInfo{ token, entry.username, remaining });
			}
		}
		return infos;
	}
};

class userManagement : public TokenManagement {
public:
	struct ExecutionDebugInfo {
		std::string execution;
		long long remainingSeconds;
	};

	userManagement(DatabaseConnection& dbConn) : dbConn_(dbConn) {}

	void initialize() {
	}

	bool dbAddNewUser(const std::string& username, const std::string& passwordHash) {
		std::map<std::string, std::string> fields = {
			{"username", username},
			{"password", passwordHash}
		};
		return dbConn_.insert("user_info", fields) != -1;
	}

	std::string dbGetUserPasswordHash(const std::string& username) {
		auto result = dbConn_.fetchOne("user_info", "username", username);
		if (result.empty()) {
			return "";
		}
		return result["password"];
	}

	bool dbUserExists(const std::string& username) {
		if (dbConn_.fetchOne("user_info", "username", username).empty()) {
			return false;
		}
		return true;
	}

	bool dbDeleteUser(const std::string& username) {
		const int deleted = dbConn_.remove("user_info", "username = ?", { username });
		return deleted != -1;
	}

	bool initializeLoginSecurity() {
		constexpr const char* kPrivateKeyPath = "./keys/private.pem";
		constexpr const char* kPublicKeyPath = "./keys/public.pem";

		std::unique_ptr<BIO, decltype(&BIO_free)> privateBio(BIO_new_file(kPrivateKeyPath, "rb"), BIO_free);
		if (!privateBio) {
			return false;
		}

	   EVP_PKEY* rawPrivateKey = PEM_read_bio_PrivateKey(privateBio.get(), nullptr, nullptr, nullptr);
		if (rawPrivateKey == nullptr) {
			return false;
		}
		std::shared_ptr<EVP_PKEY> loadedPrivateKey(rawPrivateKey, EVP_PKEY_free);

		std::ifstream publicFile(kPublicKeyPath, std::ios::binary);
		if (!publicFile.is_open()) {
			return false;
		}
		const std::string loadedPublicPem((std::istreambuf_iterator<char>(publicFile)), std::istreambuf_iterator<char>());
		if (loadedPublicPem.empty()) {
			return false;
		}

		std::unique_ptr<BIO, decltype(&BIO_free)> publicBio(BIO_new_mem_buf(loadedPublicPem.data(), static_cast<int>(loadedPublicPem.size())), BIO_free);
		if (!publicBio) {
			return false;
		}

		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> parsedPublicKey(PEM_read_bio_PUBKEY(publicBio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
		if (!parsedPublicKey) {
			return false;
		}

		std::scoped_lock lock(loginMutex_);
		publicKeyPem_ = loadedPublicPem;
		privateKey_ = std::move(loadedPrivateKey);
		pendingExecutions_.clear();
		return true;
	}

	std::string generateExecution() {
		std::array<std::uint8_t, 32> randomBytes{};
		std::random_device rd;
		for (auto& b : randomBytes) {
			b = static_cast<std::uint8_t>(rd());
		}

		const std::string execution = Sha256::Hex(std::span<const std::uint8_t>(randomBytes.data(), randomBytes.size()));
		std::scoped_lock lock(loginMutex_);
		clearExpiredExecutions();
		pendingExecutions_[execution] = getExecutionExpiresAt();
		return execution;
	}

	std::string getPublicKeyPem() const {
		std::scoped_lock lock(loginMutex_);
		return publicKeyPem_;
	}

	bool encryptWithPublicKey(const std::string& plainText, std::string& encryptedBase64, std::string& info) {
		if (plainText.empty()) {
			info = "Plain text is empty.";
			return false;
		}

		std::string publicPem;
		{
			std::scoped_lock lock(loginMutex_);
			publicPem = publicKeyPem_;
		}

		if (publicPem.empty()) {
			info = "Public key is not initialized.";
			return false;
		}

		std::unique_ptr<BIO, decltype(&BIO_free)> publicBio(BIO_new_mem_buf(publicPem.data(), static_cast<int>(publicPem.size())), BIO_free);
		if (!publicBio) {
			info = "Failed to create public key BIO.";
			return false;
		}

		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> publicKey(PEM_read_bio_PUBKEY(publicBio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
		if (!publicKey) {
			info = "Failed to parse public key.";
			return false;
		}

		std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> encryptCtx(EVP_PKEY_CTX_new(publicKey.get(), nullptr), EVP_PKEY_CTX_free);
		if (!encryptCtx) {
			info = "Failed to create encrypt context.";
			return false;
		}

		if (EVP_PKEY_encrypt_init(encryptCtx.get()) <= 0) {
			info = "Encrypt init failed.";
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_padding(encryptCtx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) {
			info = "Set RSA padding failed.";
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_oaep_md(encryptCtx.get(), EVP_sha256()) <= 0) {
			info = "Set OAEP md failed.";
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_mgf1_md(encryptCtx.get(), EVP_sha256()) <= 0) {
			info = "Set MGF1 md failed.";
			return false;
		}

		size_t encryptedLen = 0;
		if (EVP_PKEY_encrypt(encryptCtx.get(), nullptr, &encryptedLen,
			reinterpret_cast<const unsigned char*>(plainText.data()), plainText.size()) <= 0) {
			info = "Failed to get encrypted length.";
			return false;
		}

		std::vector<unsigned char> encrypted(encryptedLen);
		if (EVP_PKEY_encrypt(encryptCtx.get(), encrypted.data(), &encryptedLen,
			reinterpret_cast<const unsigned char*>(plainText.data()), plainText.size()) <= 0) {
			info = "Encrypt failed.";
			return false;
		}

		encryptedBase64 = Base64::Encode(std::string(reinterpret_cast<const char*>(encrypted.data()), encryptedLen));
		info.clear();
		return true;
	}

	bool verifyLogin(const std::string& username, const std::string& varify, std::string& token, std::string& info) {
		if (username.empty() || varify.empty()) {
           info = "Missing Required Fields.";
			return false;
		}

		if (!dbUserExists(username)) {
			info = "User Not Exists.";
			return false;
		}

		std::string passwordHash = dbGetUserPasswordHash(username);

		std::shared_ptr<EVP_PKEY> privateKey;
		{
			std::scoped_lock lock(loginMutex_);
			privateKey = privateKey_;
		}

		std::string plainText;
		if (!decryptVarify(privateKey.get(), varify, plainText)) {
			info = "Decryption Failed.";
			return false;
		}

		std::string matchedExecution;
		{
			std::scoped_lock lock(loginMutex_);
			clearExpiredExecutions();
			for (const auto& [execution, expiresAt] : pendingExecutions_) {
				if (plainText == username + passwordHash + execution) {
					matchedExecution = execution;
					break;
				}
			}

			if (matchedExecution.empty()) {
				info = "No Matching Execution Found.";
				return false;
			}

			pendingExecutions_.erase(matchedExecution);
		}

		token = storeNewToken(username, matchedExecution);
		return true;
	}

	bool registerUser(const std::string& username, const std::string& encryptedPassword, const std::string& execution, std::string& info) {
		if (username.empty() || encryptedPassword.empty() || execution.empty()) {
			return false;
		}

		std::string existsPassword;
		if (dbUserExists(username)) {
			info = "User Already Exists.";
			return false;
		}

		std::shared_ptr<EVP_PKEY> privateKey;
		{
			std::scoped_lock lock(loginMutex_);
			clearExpiredExecutions();
			auto it = pendingExecutions_.find(execution);
			if (it == pendingExecutions_.end()) {
				info = "No Matching Execution Found.";
				return false;
			}
			pendingExecutions_.erase(it);
			privateKey = privateKey_;
		}

		std::string plainText;
		if (!decryptVarify(privateKey.get(), encryptedPassword, plainText)) {
			info = "Decryption Failed.";
			return false;
		}

		if (plainText.size() <= username.size()) {
			info = "Decrypted Text Too Short.";
			return false;
		}

		if (plainText.rfind(username, 0) != 0) {
			info = "Username Mismatch in Decrypted Text.";
			return false;
		}

		const std::string passwordHash = plainText.substr(username.size());
		if (!isHexSha256(passwordHash)) {
			info = "Decrypted Password Hash Format Invalid.";
			return false;
		}

		return dbAddNewUser(username, passwordHash);
	}

	std::vector<ExecutionDebugInfo> getExecutionDebugInfo() {
		std::vector<ExecutionDebugInfo> infos;
		std::scoped_lock lock(loginMutex_);
		clearExpiredExecutions();
		const auto now = std::chrono::steady_clock::now();
		infos.reserve(pendingExecutions_.size());
		for (const auto& [execution, expiresAt] : pendingExecutions_) {
			const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(expiresAt - now).count();
			if (remaining > 0) {
				infos.push_back(ExecutionDebugInfo{ execution, remaining });
			}
		}
		return infos;
	}

private:
	static bool decryptVarify(EVP_PKEY* privateKey, const std::string& encryptedBase64, std::string& plainText) {
		if (privateKey == nullptr || encryptedBase64.empty()) {
			return false;
		}

		std::string encrypted;
		if (!Base64::Decode(encryptedBase64, encrypted).empty() || encrypted.empty()) {
			return false;
		}

		std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> decryptCtx(EVP_PKEY_CTX_new(privateKey, nullptr), EVP_PKEY_CTX_free);
		if (!decryptCtx) {
			return false;
		}

		if (EVP_PKEY_decrypt_init(decryptCtx.get()) <= 0) {
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_padding(decryptCtx.get(), RSA_PKCS1_OAEP_PADDING) <= 0) {
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_oaep_md(decryptCtx.get(), EVP_sha256()) <= 0) {
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_mgf1_md(decryptCtx.get(), EVP_sha256()) <= 0) {
			return false;
		}

		size_t plainLen = 0;
		if (EVP_PKEY_decrypt(decryptCtx.get(), nullptr, &plainLen,
			reinterpret_cast<const unsigned char*>(encrypted.data()), encrypted.size()) <= 0) {
			return false;
		}

		std::string plain(plainLen, '\0');
		if (EVP_PKEY_decrypt(decryptCtx.get(), reinterpret_cast<unsigned char*>(plain.data()), &plainLen,
			reinterpret_cast<const unsigned char*>(encrypted.data()), encrypted.size()) <= 0) {
			return false;
		}

		plain.resize(plainLen);
		plainText = std::move(plain);
		return true;
	}

	static bool isHexSha256(const std::string& text) {
		if (text.size() != 64) {
			return false;
		}

		for (const unsigned char ch : text) {
			if (std::isxdigit(ch) == 0) {
				return false;
			}
		}

		return true;
	}

	std::chrono::steady_clock::time_point getExecutionExpiresAt() const {
		return std::chrono::steady_clock::now() + executionTtl_;
	}

	void clearExpiredExecutions() {
		const auto now = std::chrono::steady_clock::now();
		for (auto it = pendingExecutions_.begin(); it != pendingExecutions_.end();) {
			if (it->second <= now) {
				it = pendingExecutions_.erase(it);
			}
			else {
				++it;
			}
		}
	}

private:
 DatabaseConnection& dbConn_;
	mutable std::mutex loginMutex_;
 std::unordered_map<std::string, std::chrono::steady_clock::time_point> pendingExecutions_;
 std::chrono::minutes executionTtl_{ 5 };
	std::string publicKeyPem_;
	std::shared_ptr<EVP_PKEY> privateKey_;
};

#endif // !_TOKEN_MANAGEMENT_H_