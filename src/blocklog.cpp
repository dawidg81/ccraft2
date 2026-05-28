#include "blocklog.hpp"
#include "Logger.hpp"

BlockLog blockLog("db/blocklog.db");

BlockLog::BlockLog(const std::string& path) {
	int rc = sqlite3_open(path.c_str(), &db);
	if (rc) {
		logger.err("Cannot open blocklog database: " + std::string(sqlite3_errmsg(db)));
		sqlite3_close(db);
		db = nullptr;
		return;
	}
	createSchema();
}

BlockLog::~BlockLog() {
	if (db) sqlite3_close(db);
}

void BlockLog::createSchema() {
	execOrThrow(
			"CREATE TABLE IF NOT EXISTS block_changes ("
			"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"  user_id INTEGER NOT NULL,"
			"  world_id INTEGER NOT NULL,"
			"  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"  block_id INTEGER NOT NULL"
			");"
		   );
}

void BlockLog::execOrThrow(const std::string& sql) const {
	if (!db) return;
	char* err = nullptr;
	int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
	if (rc != SQLITE_OK) {
		logger.err("BlockLog SQL error: " + std::string(err ? err : "unknown"));
		if (err) sqlite3_free(err);
	}
}

void BlockLog::logBlockChange(int64_t userId, int64_t worldId, uint8_t blockId) {
	if (!db) return;

	std::string sql = 
		"INSERT INTO block_changes (user_id, world_id, block_id) VALUES (" +
		std::to_string(userId) + ", " +
		std::to_string(worldId) + ", " +
		std::to_string(blockId) + ");";

	execOrThrow(sql);
}
