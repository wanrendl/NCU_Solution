#ifndef _DATABASE_CONNECTION_H_
#define _DATABASE_CONNECTION_H_

#include "Logger.h"

#ifdef _WIN32
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>
#else
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>
#include <mysql_driver.h>
#endif
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <cctype>

#ifdef _MSC_VER
#pragma comment(lib, "../../../../libraries/MySQL/lib64/vs14/mysqlcppconn.lib")
#endif

/**
 * @brief 数据库连接与初始化器
 * 负责创建统一数据库及其所有表，并提供查询功能。
 */
class DatabaseConnection {
public:
	/**
	 * @brief 构造函数
	 * @param host       MySQL 服务器地址，如 "tcp://127.0.0.1:3306"
	 * @param user       用户名
	 * @param password   密码
	 * @param dbName     要创建的统一数据库名称
	 */
	DatabaseConnection(const std::string& host,
		const std::string& user,
		const std::string& password,
		const std::string& dbName)
		: host_(host), user_(user), password_(password), dbName_(dbName), logger_(nullptr) {}

	// 将 Logger 的引用注入（内部只保存指针，不进行拷贝/赋值）
	void setLogger(Logger& logger) {
		logger_ = &logger;
	}

	/**
	 * @brief 初始化数据库及所有表（若已存在则跳过）
	 * @return true 成功，false 失败
	 */
	bool initialize() {
		if (logger_) logger_->Info("Checking database and tables.");
		if (verify()) return true;

		sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
		if (!driver) {
			if (logger_) logger_->Error("Failed to get MySQL driver instance.");
			return false;
		}

		std::unique_ptr<sql::Connection> conn;

		try {
			// 1. 先不指定数据库，连接 MySQL 服务器
			conn.reset(driver->connect(host_, user_, password_));
			configureUtf8mb4(conn.get());
			if (logger_) logger_->Info("Connected to MySQL server.");

			// 2. 创建数据库
			if (!createDatabase(conn.get())) {
				return false;
			}

			// 3. 切换到刚刚创建（或已存在）的数据库
			conn->setSchema(dbName_);
			if (logger_) logger_->Info("Switched to database: " + dbName_);

			// 4. 创建所有表
			if (!createTables(conn.get())) {
				return false;
			}

			if (logger_) logger_->Info("Database initialization completed successfully.");
			return true;

		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("SQL error: ") + e.what() +
				" (Error code: " + std::to_string(e.getErrorCode()) +
				", SQLState: " + std::string(e.getSQLState()) + ")");
			return false;
		}
		catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Standard error: ") + e.what());
			return false;
		}
	}

	/**
	 * @brief 验证数据库和所有表是否已正确创建
	 * @return true 全部存在，false 有缺失
	 */
	bool verify() const {
		sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
		if (!driver) {
			if (logger_) logger_->Error("Failed to get MySQL driver instance.");
			return false;
		}

		try {
			std::unique_ptr<sql::Connection> conn(driver->connect(host_, user_, password_));
          configureUtf8mb4(conn.get());
			if (logger_) logger_->Info("Connected to MySQL server for verification.");

			// 检查数据库是否存在
			std::unique_ptr<sql::PreparedStatement> pstmt(
				conn->prepareStatement("SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = ?"));
			pstmt->setString(1, dbName_);
			std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

			if (!res->next()) {
				if (logger_) logger_->Error("Database '" + dbName_ + "' does not exist.");
				return false;
			}
			if (logger_) logger_->Info("Database '" + dbName_ + "' exists.");

			const std::map<std::string, std::vector<std::string>> requiredSchema = {
				{"user_info", {"id", "username", "password", "email", "created_at"}},
				{"config", {"id", "config_key", "config_value", "updated_at"}},
				{"reservations_processing", {"unique_id", "reservation_madetime", "reservation_court", "reservation_date", "reservation_time"}},
				{"reservations_finished", {"id", "status", "reservation_court", "reservation_date", "reservation_time", "details", "finished_at"}},
				{"converter_pending", {"unique_id", "user_id", "file_name", "file_size", "status"}},
				{"converter_finished", {"user_id", "file_name", "file_raw_name", "file_format", "file_size", "picture_hash", "finished_at"}}
			};

			// 读取当前数据库所有表
			pstmt.reset(conn->prepareStatement(
				"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = ?"));
			pstmt->setString(1, dbName_);
			res.reset(pstmt->executeQuery());

			std::set<std::string> existingTables;
			while (res->next()) {
				existingTables.insert(res->getString("TABLE_NAME"));
			}

			bool allExist = true;

			for (const auto& entry : requiredSchema) {
				const std::string& tableName = entry.first;
				const std::vector<std::string>& requiredColumns = entry.second;

				if (existingTables.find(tableName) == existingTables.end()) {
					if (logger_) logger_->Error("Table '" + tableName + "' is missing.");
					allExist = false;
					continue;
				}
				if (logger_) logger_->Info("Table '" + tableName + "' exists.");

				pstmt.reset(conn->prepareStatement(
					"SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ?"));
				pstmt->setString(1, dbName_);
				pstmt->setString(2, tableName);
				res.reset(pstmt->executeQuery());

				std::set<std::string> existingColumns;
				while (res->next()) {
					existingColumns.insert(res->getString("COLUMN_NAME"));
				}

				for (const auto& columnName : requiredColumns) {
					if (existingColumns.find(columnName) == existingColumns.end()) {
						if (logger_) logger_->Error("Column '" + tableName + "." + columnName + "' is missing.");
						allExist = false;
					}
				}
			}

			if (allExist) {
				if (logger_) logger_->Info("All required tables and columns exist.");
			}
			else {
				if (logger_) logger_->Error("Some required tables or columns are missing.");
			}

			return allExist;

		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("SQL error during verification: ") + e.what() +
				" (Error code: " + std::to_string(e.getErrorCode()) +
				", SQLState: " + std::string(e.getSQLState()) + ")");
			return false;
		}
		catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Standard error during verification: ") + e.what());
			return false;
		}
	}

	// ========== 查询功能 ==========

	/**
	 * @brief 执行任意 SELECT 查询
	 * @param sql SELECT 语句
	 * @return 结果集智能指针，若失败返回 nullptr
	 */
	std::unique_ptr<sql::ResultSet> query(const std::string& sql) {
		auto conn = createConnection();
		if (!conn) return nullptr;

		try {
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			return std::unique_ptr<sql::ResultSet>(stmt->executeQuery(sql));
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("Query failed: ") + e.what());
			return nullptr;
		}
       catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Query failed: ") + e.what());
			return nullptr;
		}
	}

	/**
	 * @brief 执行 INSERT/UPDATE/DELETE 语句
	 * @param sql 更新语句
	 * @return 受影响的行数，-1 表示失败
	 */
	int update(const std::string& sql) {
		auto conn = createConnection();
		if (!conn) return -1;

		try {
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			return stmt->executeUpdate(sql);
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("Update failed: ") + e.what());
			return -1;
		}
       catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Update failed: ") + e.what());
			return -1;
		}
	}

	/**
	 * @brief 简单等值条件查询（SELECT * FROM table WHERE column = value）
	 * @param table 表名
	 * @param column 条件列名
	 * @param value 条件值
	 * @return 结果集智能指针，若失败返回 nullptr
	 */
	std::unique_ptr<sql::ResultSet> select(const std::string& table,
		const std::string& column,
		const std::string& value) {
		// 注意：为防止 SQL 注入，应对 value 进行转义，但此处为了简化，要求调用者保证安全性。
		std::string sql = "SELECT * FROM " + table + " WHERE " + column + " = '" + escapeString(value) + "'";
		return query(sql);
	}

	/**
	 * @brief 简单等值条件查询，返回单个字段的值（第一行第一列）
	 * @param table 表名
	 * @param column 要查询的字段名
	 * @param conditionColumn 条件列名
	 * @param conditionValue 条件值
	 * @return 查询到的字符串值，若未找到或出错返回空字符串
	 */
	std::string selectValue(const std::string& table,
		const std::string& column,
		const std::string& conditionColumn,
		const std::string& conditionValue) {
		std::string sql = "SELECT " + column + " FROM " + table +
			" WHERE " + conditionColumn + " = '" + escapeString(conditionValue) + "' LIMIT 1";
		auto res = query(sql);
		if (res && res->next()) {
			return res->getString(1);
		}
		return "";
	}

	/**
	 * @brief 根据条件查询某个字段的值（使用当前数据库）
	 * @param table 表名
	 * @param returnColumn 要返回的列名
	 * @param conditionColumn 条件列名
	 * @param conditionValue 条件值
	 * @return 查询到的字符串值，若未找到或失败返回空字符串
	 */
	std::string search(const std::string& table,
		const std::string& returnColumn,
		const std::string& conditionColumn,
		const std::string& conditionValue) {
		std::string sql = "SELECT " + returnColumn + " FROM " + table +
			" WHERE " + conditionColumn + " = '" + escapeString(conditionValue) + "' LIMIT 1";
		auto res = query(sql);
		if (res && res->next()) {
			return res->getString(1);
		}
		return "";
	}

	/**
	 * @brief 根据条件查询某个字段的值（指定数据库）
	 * @param database 数据库名
	 * @param table 表名
	 * @param returnColumn 要返回的列名
	 * @param conditionColumn 条件列名
	 * @param conditionValue 条件值
	 * @return 查询到的字符串值，若未找到或失败返回空字符串
	 */
	std::string search(const std::string& database,
		const std::string& table,
		const std::string& returnColumn,
		const std::string& conditionColumn,
		const std::string& conditionValue) {
		// 注意：此处需要临时切换数据库，但为了不改变当前连接的数据库，我们直接构造完整的数据库名.table
		std::string sql = "SELECT " + returnColumn + " FROM " + database + "." + table +
			" WHERE " + conditionColumn + " = '" + escapeString(conditionValue) + "' LIMIT 1";
		auto res = query(sql); // query 使用当前数据库连接，但 SQL 中显式指定了数据库名，所以可以正确执行
		if (res && res->next()) {
			return res->getString(1);
		}
		return "";
	}

	/**
	 * @brief 检查记录是否存在
	 * @param table 表名
	 * @param conditionColumn 条件列名
	 * @param conditionValue 条件值
	 * @return 如果记录存在返回 true，否则返回 false
	 */
	bool searchExists(const std::string& table,
		const std::string& conditionColumn,
		const std::string& conditionValue) {
		std::string sql = "SELECT 1 FROM " + table +
			" WHERE " + conditionColumn + " = '" + escapeString(conditionValue) + "' LIMIT 1";
		auto res = query(sql);
		return res && res->next();
	}

	bool searchExistsStatus(const std::string& table,
		const std::string& conditionColumn,
		const std::string& conditionValue,
		const std::string& statusColumn,
		const std::string& statusValue) {
		std::string sql = "SELECT 1 FROM " + table +
			" WHERE " + conditionColumn + " = '" + escapeString(conditionValue) + "'" +
			" AND " + statusColumn + " = '" + escapeString(statusValue) + "' LIMIT 1";
		auto res = query(sql);
		return res && res->next();
	}

	/**
 * @brief 插入一条记录（使用预处理语句）
 * @param table 表名
 * @param fields 字段名 -> 值的映射
 * @return 插入的自增ID（若表有自增主键），失败返回 -1
 */
	int64_t insert(const std::string& table, const std::map<std::string, std::string>& fields) {
		if (fields.empty()) {
			if (logger_) logger_->Error("Insert failed: fields map is empty.");
			return -1;
		}

		if (!isSafeIdentifier(table)) {
			if (logger_) logger_->Error("Insert failed: invalid table name.");
			return -1;
		}

		for (const auto& field : fields) {
			if (!isSafeIdentifier(field.first)) {
				if (logger_) logger_->Error("Insert failed: invalid column name '" + field.first + "'.");
				return -1;
			}
		}

		auto conn = createConnection();
		if (!conn) return -1;

		// 构建 INSERT 语句
		std::string columns, placeholders;
		for (auto it = fields.begin(); it != fields.end(); ++it) {
			columns += "`" + it->first + "`";
			placeholders += "?";
			if (std::next(it) != fields.end()) {
				columns += ", ";
				placeholders += ", ";
			}
		}
		std::string sql = "INSERT INTO `" + table + "` (" + columns + ") VALUES (" + placeholders + ")";

		try {
			std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));
			int index = 1;
			for (const auto& field : fields) {
				pstmt->setString(index++, field.second);
			}
			int affectedRows = pstmt->executeUpdate();
			if (affectedRows <= 0) {
				if (logger_) logger_->Error("Insert failed: no rows affected.");
				return -1;
			}

			// 获取最后插入的 ID（需要支持自增主键）
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
			if (res && res->next()) {
				int64_t lastId = res->getInt64(1);
				if (lastId > 0) {
					return lastId;
				}
				return 0;
			}
			return 0;
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("Insert failed: ") + e.what());
			return -1;
		}
       catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Insert failed: ") + e.what());
			return -1;
		}
	}

	/**
	 * @brief 更新记录（使用预处理语句）
	 * @param table 表名
	 * @param fields 字段名 -> 新值的映射
	 * @param condition WHERE 条件（不含 WHERE 关键字），使用 ? 占位符，例如 "id = ?"
	 * @param conditionValues 条件占位符对应的值（按顺序）
	 * @return 受影响的行数，失败返回 -1
	 */
	int update(const std::string& table,
		const std::map<std::string, std::string>& fields,
		const std::string& condition,
		const std::vector<std::string>& conditionValues) {
		if (fields.empty()) {
			if (logger_) logger_->Error("Update failed: fields map is empty.");
			return -1;
		}

		auto conn = createConnection();
		if (!conn) return -1;

		// 构建 UPDATE 语句
		std::string setClause;
		for (auto it = fields.begin(); it != fields.end(); ++it) {
			setClause += it->first + " = ?";
			if (std::next(it) != fields.end()) {
				setClause += ", ";
			}
		}
		std::string sql = "UPDATE " + table + " SET " + setClause + " WHERE " + condition;

		try {
			std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));
			int index = 1;
			// 绑定 SET 部分的值
			for (const auto& field : fields) {
				pstmt->setString(index++, field.second);
			}
			// 绑定 WHERE 条件的值
			for (const auto& val : conditionValues) {
				pstmt->setString(index++, val);
			}
			return pstmt->executeUpdate();
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("Update failed: ") + e.what());
			return -1;
		}
       catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Update failed: ") + e.what());
			return -1;
		}
	}

	/**
	 * @brief 删除记录（使用预处理语句）
	 * @param table 表名
	 * @param condition WHERE 条件（不含 WHERE 关键字），使用 ? 占位符
	 * @param conditionValues 条件占位符对应的值（按顺序）
	 * @return 受影响的行数，失败返回 -1
	 */
	int remove(const std::string& table,
		const std::string& condition,
		const std::vector<std::string>& conditionValues) {
		if (condition.empty()) {
			if (logger_) logger_->Error("Remove failed: condition is empty (would delete all rows).");
			return -1;
		}

		auto conn = createConnection();
		if (!conn) return -1;

		std::string sql = "DELETE FROM " + table + " WHERE " + condition;

		try {
			std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));
			int index = 1;
			for (const auto& val : conditionValues) {
				pstmt->setString(index++, val);
			}
			return pstmt->executeUpdate();
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("Delete failed: ") + e.what());
			return -1;
		}
       catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("Delete failed: ") + e.what());
			return -1;
		}
	}

	/**
	 * @brief 获取表中所有记录（不使用预处理语句）
	 * @param table 表名
	 * @return 结果集向量，每个元素是列名 -> 值的映射，失败返回空向量
	 */
	std::vector<std::map<std::string, std::string>> fetchAll(const std::string& table) {
		std::vector<std::map<std::string, std::string>> result;
		auto conn = createConnection();
		if (!conn) return result;

		std::string sql = "SELECT * FROM " + table;
		try {
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));

			// 获取元数据（列名）
			sql::ResultSetMetaData* meta = res->getMetaData();
			int colCount = meta->getColumnCount();

			while (res->next()) {
				std::map<std::string, std::string> row;
				for (int i = 1; i <= colCount; ++i) {
					std::string colName = meta->getColumnName(i);
					std::string value = res->getString(i); // 所有类型转为字符串
					row[colName] = value;
				}
				result.push_back(std::move(row));
			}
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("fetchAll failed: ") + e.what());
			result.clear();
		}
		catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("fetchAll failed: ") + e.what());
			result.clear();
		}
		return result;
	}

	std::map<std::string, std::string> fetchOne(const std::string& table, const std::string& conditionColumn, const std::string& conditionValue) {
		std::map<std::string, std::string> result;
		auto conn = createConnection();
		if (!conn) return result;
		std::string sql = "SELECT * FROM " + table + " WHERE " + conditionColumn + " = '" + escapeString(conditionValue) + "' LIMIT 1";
		try {
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));
			if (res->next()) {
				sql::ResultSetMetaData* meta = res->getMetaData();
				int colCount = meta->getColumnCount();
				for (int i = 1; i <= colCount; ++i) {
					std::string colName = meta->getColumnName(i);
					std::string value = res->getString(i);
					result[colName] = value;
				}
			}
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("fetchOne failed: ") + e.what());
			result.clear();
		}
		catch (std::exception& e) {
			if (logger_) logger_->Error(std::string("fetchOne failed: ") + e.what());
			result.clear();
		}
		return result;
	}

private:
	std::string host_;
	std::string user_;
	std::string password_;
	std::string dbName_;
	Logger* logger_; // 改为指针类型

	bool isSafeIdentifier(const std::string& identifier) const {
		if (identifier.empty()) return false;
		for (char c : identifier) {
			if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
				return false;
			}
		}
		return true;
	}

	// 创建数据库连接并切换到指定数据库
	std::unique_ptr<sql::Connection> createConnection() const {
		sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
		if (!driver) {
			if (logger_) logger_->Error("Failed to get MySQL driver instance.");
			return nullptr;
		}

		try {
			std::unique_ptr<sql::Connection> conn(driver->connect(host_, user_, password_));
           configureUtf8mb4(conn.get());
			conn->setSchema(dbName_);
			return conn;
		}
		catch (sql::SQLException& e) {
			if (logger_) logger_->Error(std::string("Failed to connect to database: ") + e.what());
			return nullptr;
		}
	}

	std::string escapeString(const std::string& str) const {
		std::string escaped;
		for (char c : str) {
			if (c == '\'') escaped += "\\'";
			else escaped += c;
		}
		return escaped;
	}

	bool createDatabase(sql::Connection* conn) {
		std::unique_ptr<sql::Statement> stmt(conn->createStatement());
       std::string sql = "CREATE DATABASE IF NOT EXISTS " + dbName_ + " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
		stmt->execute(sql);
      stmt->execute("ALTER DATABASE " + dbName_ + " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
		if (logger_) logger_->Info("Database '" + dbName_ + "' ensured.");
		return true;
	}

	bool createTables(sql::Connection* conn) {
		std::unique_ptr<sql::Statement> stmt(conn->createStatement());

		// 1. 用户表
		std::string sql_user = R"(
			CREATE TABLE IF NOT EXISTS user_info (
				id INT AUTO_INCREMENT PRIMARY KEY,
				username VARCHAR(50) NOT NULL UNIQUE,
				password VARCHAR(255) NOT NULL,
				email VARCHAR(100) NOT NULL,
				created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
			) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
		)";
		stmt->execute(sql_user);
		if (logger_) logger_->Info("Table 'user_info' ensured.");

		// 2. 配置表
		std::string sql_config = R"(
			CREATE TABLE IF NOT EXISTS config (
				id INT AUTO_INCREMENT PRIMARY KEY,
				config_key VARCHAR(100) NOT NULL UNIQUE,
				config_value TEXT,
				updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
			) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
		)";
		stmt->execute(sql_config);
		if (logger_) logger_->Info("Table 'config' ensured.");

		// 3. 预约处理中表
		std::string sql_reservations_processing = R"(
			CREATE TABLE IF NOT EXISTS reservations_processing (
				unique_id VARCHAR(64) NOT NULL,
				reservation_madetime VARCHAR(12) NOT NULL,
				reservation_court TINYINT NOT NULL,
				reservation_date DATE NOT NULL,
				reservation_time TINYINT NOT NULL
			) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
		)";
		stmt->execute(sql_reservations_processing);
		if (logger_) logger_->Info("Table 'reservations_processing' ensured.");

		// 4. 预约已完成表
		std::string sql_reservations_finished = R"(
			CREATE TABLE IF NOT EXISTS reservations_finished (
				id INT AUTO_INCREMENT PRIMARY KEY,
				status TINYINT NOT NULL,
				reservation_court TINYINT NOT NULL,
				reservation_date DATE NOT NULL,
				reservation_time TINYINT NOT NULL,
				details TEXT,
				finished_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
			) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
		)";
		stmt->execute(sql_reservations_finished);
		if (logger_) logger_->Info("Table 'reservations_finished' ensured.");

		// 5. 转换待处理表
		std::string sql_converter_pending = R"(
			CREATE TABLE IF NOT EXISTS converter_pending (
				unique_id VARCHAR(64) NOT NULL,
				user_id VARCHAR(64) NOT NULL,
				file_name VARCHAR(255) NOT NULL,
				file_size BIGINT NOT NULL,
				status TINYINT DEFAULT 0
			) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
		)";
		stmt->execute(sql_converter_pending);
		if (logger_) logger_->Info("Table 'converter_pending' ensured.");

		// 6. 转换已完成表
		std::string sql_converter_finished = R"(
			CREATE TABLE IF NOT EXISTS converter_finished (
				user_id VARCHAR(64) NOT NULL,
				file_name VARCHAR(64) NOT NULL,
				file_raw_name TEXT NOT NULL,
				file_size BIGINT NOT NULL,
				file_format VARCHAR(5) NOT NULL,
				picture_hash VARCHAR(64) NOT NULL,
				finished_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
			) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
		)";
		stmt->execute(sql_converter_finished);
		if (logger_) logger_->Info("Table 'converter_finished' ensured.");

		return true;
	}

	static void configureUtf8mb4(sql::Connection* conn) {
		if (!conn) return;
      try {
			conn->setClientOption("OPT_CHARSET_NAME", "utf8mb4");
		}
		catch (...) {}
		std::unique_ptr<sql::Statement> stmt(conn->createStatement());
		stmt->execute("SET NAMES utf8mb4");
		stmt->execute("SET character_set_client = utf8mb4");
		stmt->execute("SET character_set_connection = utf8mb4");
		stmt->execute("SET character_set_results = utf8mb4");
	}


};

#endif // _DATABASE_CONNECTION_H_