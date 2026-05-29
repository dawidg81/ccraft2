#pragma once
#include <string>
#include <cstdint>
#include "sqlite3.h"
#include "logger_instance.hpp"

class BlockLog {
	public:
		explicit BlockLog(const std::string& path = "db/blocklog.db");
		~BlockLog();

		BlockLog(const BlockLog&)            = delete;
		BlockLog& operator=(const BlockLog&) = delete;

		// worldId should be the rowid from worldDB
		void logBlockChange(int64_t userId, int64_t worldId, uint8_t blockId);

	private:
		sqlite3* db = nullptr;

		void execOrThrow(const std::string& sql) const;
		void createSchema();
};

extern BlockLog blockLog;
