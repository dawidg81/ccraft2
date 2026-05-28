#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "sqlite3.h"

struct WorldRecord {
	int64_t     rowid       = 0;
	std::string name;
	int32_t     sizeX       = 256;
	int32_t     sizeY       = 64;
	int32_t     sizeZ       = 256;
	std::string createdAt;  // ISO-8601
};

class WorldDB {
	public:
		explicit WorldDB(const std::string& path = "db/worlds.db");
		~WorldDB();

		WorldDB(const WorldDB&)            = delete;
		WorldDB& operator=(const WorldDB&) = delete;

		// Create a new world and return its rowid
		int64_t createWorld(const std::string& name, int32_t sizeX, int32_t sizeY, int32_t sizeZ);

		// Get world by name
		bool getByName(const std::string& name, WorldRecord& out) const;

		// Get world by rowid
		bool getById(int64_t rowid, WorldRecord& out) const;

		// List all worlds
		std::vector<WorldRecord> listAll() const;

	private:
		sqlite3* db = nullptr;

		void execOrThrow(const std::string& sql) const;
		void createSchema();
};

extern WorldDB worldDB;
