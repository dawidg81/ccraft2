#include "worlddb.hpp"
#include "Logger.hpp"
#include "logger_instance.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

WorldDB worldDB("db/worlds.db");

static std::string getCurrentTimestamp() {
	auto now = std::time(nullptr);
	auto tm = *std::gmtime(&now);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
	return oss.str();
}

WorldDB::WorldDB(const std::string& path) {
	int rc = sqlite3_open(path.c_str(), &db);
	if (rc) {
		logger.err("Cannot open world database: " + std::string(sqlite3_errmsg(db)));
		sqlite3_close(db);
		db = nullptr;
		return;
	}
	createSchema();
}

WorldDB::~WorldDB() {
	if (db) sqlite3_close(db);
}

void WorldDB::createSchema() {
	execOrThrow(
			"CREATE TABLE IF NOT EXISTS worlds ("
			"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"  name TEXT UNIQUE NOT NULL,"
			"  size_x INTEGER NOT NULL,"
			"  size_y INTEGER NOT NULL,"
			"  size_z INTEGER NOT NULL,"
			"  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
			");"
		   );
}

void WorldDB::execOrThrow(const std::string& sql) const {
	if (!db) return;
	char* err = nullptr;
	int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
	if (rc != SQLITE_OK) {
		logger.err("WorldDB SQL error: " + std::string(err ? err : "unknown"));
		if (err) sqlite3_free(err);
	}
}

int64_t WorldDB::createWorld(const std::string& name, int32_t sizeX, int32_t sizeY, int32_t sizeZ) {
	if (!db) return -1;

	std::string sql =
		"INSERT INTO worlds (name, size_x, size_y, size_z) VALUES ('" +
		name + "', " +
		std::to_string(sizeX) + ", " +
		std::to_string(sizeY) + ", " +
		std::to_string(sizeZ) + ");";

	execOrThrow(sql);
	return sqlite3_last_insert_rowid(db);
}

bool WorldDB::getByName(const std::string& name, WorldRecord& out) const {
	if (!db) return false;

	std::string sql = "SELECT id, name, size_x, size_y, size_z, created_at FROM worlds WHERE name = '" + name + "';";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		logger.err("WorldDB prepare error: " + std::string(sqlite3_errmsg(db)));
		return false;
	}

	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		out.rowid = sqlite3_column_int64(stmt, 0);
		out.name = std::string((const char*)sqlite3_column_text(stmt, 1));
		out.sizeX = sqlite3_column_int(stmt, 2);
		out.sizeY = sqlite3_column_int(stmt, 3);
		out.sizeZ = sqlite3_column_int(stmt, 4);
		out.createdAt = std::string((const char*)sqlite3_column_text(stmt, 5));
		found = true;
	}
	sqlite3_finalize(stmt);
	return found;
}

bool WorldDB::getById(int64_t rowid, WorldRecord& out) const {
	if (!db) return false;

	std::string sql = "SELECT id, name, size_x, size_y, size_z, created_at FROM worlds WHERE id = " + std::to_string(rowid) + ";";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		logger.err("WorldDB prepare error: " + std::string(sqlite3_errmsg(db)));
		return false;
	}

	bool found = false;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		out.rowid = sqlite3_column_int64(stmt, 0);
		out.name = std::string((const char*)sqlite3_column_text(stmt, 1));
		out.sizeX = sqlite3_column_int(stmt, 2);
		out.sizeY = sqlite3_column_int(stmt, 3);
		out.sizeZ = sqlite3_column_int(stmt, 4);
		out.createdAt = std::string((const char*)sqlite3_column_text(stmt, 5));
		found = true;
	}
	sqlite3_finalize(stmt);
	return found;
}

std::vector<WorldRecord> WorldDB::listAll() const {
	std::vector<WorldRecord> result;
	if (!db) return result;

	const char* sql = "SELECT id, name, size_x, size_y, size_z, created_at FROM worlds;";
	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		logger.err("WorldDB prepare error: " + std::string(sqlite3_errmsg(db)));
		return result;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		WorldRecord rec;
		rec.rowid = sqlite3_column_int64(stmt, 0);
		rec.name = std::string((const char*)sqlite3_column_text(stmt, 1));
		rec.sizeX = sqlite3_column_int(stmt, 2);
		rec.sizeY = sqlite3_column_int(stmt, 3);
		rec.sizeZ = sqlite3_column_int(stmt, 4);
		rec.createdAt = std::string((const char*)sqlite3_column_text(stmt, 5));
		result.push_back(rec);
	}
	sqlite3_finalize(stmt);
	return result;
}
