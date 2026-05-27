#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "sqlite3.h"

struct PlayerRecord {
    int64_t     rowid       = 0;
    std::string username;
    std::vector<std::string> aliases;
    std::vector<std::string> ips;
    bool        isOp        = false;
    bool        isBanned    = false;
    std::string banReason;
    std::string firstSeen;              // ISO-8601
    std::string lastSeen;
};

class PlayerDB {
public:
    explicit PlayerDB(const std::string& path = "db/players.db");
    ~PlayerDB();

    PlayerDB(const PlayerDB&)            = delete;
    PlayerDB& operator=(const PlayerDB&) = delete;

    PlayerRecord loginPlayer(const std::string& username, const std::string& ip);

    bool getByUsername(const std::string& username, PlayerRecord& out) const;

    bool getById(int64_t rowid, PlayerRecord& out) const;

    bool setOp    (const std::string& username, bool op);
    bool setBanned(const std::string& username, bool banned, const std::string& reason = "");

    bool isOp    (const std::string& username) const;
    bool isBanned(const std::string& username) const;

    std::vector<PlayerRecord> listAll() const;

private:
    sqlite3* db = nullptr;

    void execOrThrow(const std::string& sql) const;
    void createSchema();

    void loadRelations(PlayerRecord& rec) const;

    bool findPlayerRowid(const std::string& username, int64_t& rowid) const;
};

extern PlayerDB playerDB;