#include "playerdb.hpp"
#include <ctime>
#include <stdexcept>
#include <algorithm>
#include <sstream>

PlayerDB playerDB;

static std::string isoNow() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

PlayerDB::PlayerDB(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::string err = db ? sqlite3_errmsg(db) : "unknown";
        sqlite3_close(db);
        db = nullptr;
        throw std::runtime_error("PlayerDB: cannot open database: " + err);
    }
    execOrThrow("PRAGMA journal_mode=WAL;");
    execOrThrow("PRAGMA foreign_keys=ON;");
    createSchema();
}

PlayerDB::~PlayerDB() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

// ── schema ───────────────────────────────────────────────────────────────────
void PlayerDB::createSchema() {
    execOrThrow(R"sql(
        CREATE TABLE IF NOT EXISTS players (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            username    TEXT    NOT NULL UNIQUE COLLATE NOCASE,
            is_op       INTEGER NOT NULL DEFAULT 0,
            is_banned   INTEGER NOT NULL DEFAULT 0,
            ban_reason  TEXT    NOT NULL DEFAULT '',
            first_seen  TEXT    NOT NULL,
            last_seen   TEXT    NOT NULL
        );
    )sql");

    execOrThrow(R"sql(
        CREATE TABLE IF NOT EXISTS player_aliases (
            player_id   INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,
            alias       TEXT    NOT NULL COLLATE NOCASE,
            PRIMARY KEY (player_id, alias)
        );
    )sql");

    execOrThrow(R"sql(
        CREATE TABLE IF NOT EXISTS player_ips (
            player_id   INTEGER NOT NULL REFERENCES players(id) ON DELETE CASCADE,
            ip          TEXT    NOT NULL,
            PRIMARY KEY (player_id, ip)
        );
    )sql");
}

PlayerRecord PlayerDB::loginPlayer(const std::string& username, const std::string& ip) {
    std::string now = isoNow();
    int64_t rowid = 0;

    if (!findPlayerRowid(username, rowid)) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "INSERT INTO players (username, is_op, is_banned, ban_reason, first_seen, last_seen) "
            "VALUES (?, 0, 0, '', ?, ?);";
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, now.c_str(),      -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, now.c_str(),      -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        rowid = sqlite3_last_insert_rowid(db);
    } else {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "UPDATE players SET last_seen=? WHERE id=?;", -1, &stmt, nullptr);
        sqlite3_bind_text (stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, rowid);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT OR IGNORE INTO player_aliases (player_id, alias) VALUES (?, ?);",
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, rowid);
        sqlite3_bind_text (stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    if (!ip.empty()) {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT OR IGNORE INTO player_ips (player_id, ip) VALUES (?, ?);",
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, rowid);
        sqlite3_bind_text (stmt, 2, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    PlayerRecord rec;
    getById(rowid, rec);
    return rec;
}

bool PlayerDB::getByUsername(const std::string& username, PlayerRecord& out) const {
    int64_t rowid = 0;
    if (!findPlayerRowid(username, rowid)) return false;
    return getById(rowid, out);
}

bool PlayerDB::getById(int64_t rowid, PlayerRecord& out) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT id, username, is_op, is_banned, ban_reason, first_seen, last_seen "
        "FROM players WHERE id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, rowid);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found             = true;
        out.rowid         = sqlite3_column_int64(stmt, 0);
        out.username      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.isOp          = sqlite3_column_int(stmt, 2) != 0;
        out.isBanned      = sqlite3_column_int(stmt, 3) != 0;
        out.banReason     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        out.firstSeen     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        out.lastSeen      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    }
    sqlite3_finalize(stmt);

    if (found) loadRelations(out);
    return found;
}

bool PlayerDB::setOp(const std::string& username, bool op) {
    int64_t rowid = 0;
    if (!findPlayerRowid(username, rowid)) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "UPDATE players SET is_op=? WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_int  (stmt, 1, op ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, rowid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return true;
}

bool PlayerDB::setBanned(const std::string& username, bool banned, const std::string& reason) {
    int64_t rowid = 0;
    if (!findPlayerRowid(username, rowid)) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "UPDATE players SET is_banned=?, ban_reason=? WHERE id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int  (stmt, 1, banned ? 1 : 0);
    sqlite3_bind_text (stmt, 2, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, rowid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return true;
}

bool PlayerDB::isOp(const std::string& username) const {
    int64_t rowid = 0;
    if (!findPlayerRowid(username, rowid)) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT is_op FROM players WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, rowid);
    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0) != 0;
    sqlite3_finalize(stmt);
    return result;
}

bool PlayerDB::isBanned(const std::string& username) const {
    int64_t rowid = 0;
    if (!findPlayerRowid(username, rowid)) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT is_banned FROM players WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, rowid);
    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0) != 0;
    sqlite3_finalize(stmt);
    return result;
}

std::vector<PlayerRecord> PlayerDB::listAll() const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT id, username, is_op, is_banned, ban_reason, first_seen, last_seen "
        "FROM players ORDER BY last_seen DESC;",
        -1, &stmt, nullptr);

    std::vector<PlayerRecord> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PlayerRecord rec;
        rec.rowid     = sqlite3_column_int64(stmt, 0);
        rec.username  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.isOp      = sqlite3_column_int(stmt, 2) != 0;
        rec.isBanned  = sqlite3_column_int(stmt, 3) != 0;
        rec.banReason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        rec.firstSeen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        rec.lastSeen  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        loadRelations(rec);
        result.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return result;
}

void PlayerDB::execOrThrow(const std::string& sql) const {
    char* errmsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("PlayerDB SQL error: " + err);
    }
}

void PlayerDB::loadRelations(PlayerRecord& rec) const {
    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "SELECT alias FROM player_aliases WHERE player_id=?;",
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, rec.rowid);
        rec.aliases.clear();
        while (sqlite3_step(stmt) == SQLITE_ROW)
            rec.aliases.emplace_back(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        sqlite3_finalize(stmt);
    }
    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "SELECT ip FROM player_ips WHERE player_id=?;",
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, rec.rowid);
        rec.ips.clear();
        while (sqlite3_step(stmt) == SQLITE_ROW)
            rec.ips.emplace_back(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        sqlite3_finalize(stmt);
    }
}

bool PlayerDB::findPlayerRowid(const std::string& username, int64_t& rowid) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT p.id FROM players p "
        "LEFT JOIN player_aliases a ON a.player_id = p.id "
        "WHERE p.username=? COLLATE NOCASE OR a.alias=? COLLATE NOCASE "
        "LIMIT 1;",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rowid = sqlite3_column_int64(stmt, 0);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}