#include "conf.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

ServerConfig gConfig;

static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string stripComment(const std::string& s) {
    bool inQuote = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') { inQuote = !inQuote; continue; }
        if (!inQuote && (s[i] == '#' || s[i] == ';'))
            return trim(s.substr(0, i));
    }
    return s;
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

static bool parseBool(const std::string& v) {
    std::string lo = v;
    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
    return lo == "true" || lo == "1" || lo == "yes";
}

static const char* DEFAULT_TOML = R"([server]
name    = "Default Server"
motd    = "Welcome!"
max_players = 256

[network]
bind_address = "0.0.0.0"
port         = 25565

[heartbeat]
host             = "www.classicube.net"
path             = "/server/heartbeat/"
port             = 80
public           = true
interval_minutes = 1

[save]
interval_minutes = 5
)";

bool loadConfig(const std::string& path) {
    bool exists = false;
    {
        std::ifstream probe(path);
        exists = probe.is_open();
    }

    if (!exists) {
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "[conf] Warning: could not create default " << path
                      << " (errno " << errno << ": " << strerror(errno) << ")\n";
        } else {
            out << DEFAULT_TOML;
            out.flush();
            if (out.fail()) {
                std::cerr << "[conf] Warning: failed to write default " << path
                        << " (errno " << errno << ": " << strerror(errno) << ")\n";
            } else {
                std::cout << "[conf] Created default " << path << "\n";
            }
        }
        return true;
    } else {
        std::cout << "[conf] Created default " << path << "\n";
        std::ifstream verify(path);
        if (!verify.is_open())
            std::cerr << "[conf] Warning: file created but cannot be re-read!\n";
    }

    std::ifstream f(path);
    if (!f) {
        std::cerr << "[conf] Error: cannot open " << path << "\n";
        return false;
    }

    std::string section;
    std::string line;
    int lineNo = 0;

    while (std::getline(f, line)) {
        ++lineNo;
        line = trim(stripComment(line));
        if (line.empty()) continue;

        if (line.front() == '[') {
            auto close = line.find(']');
            if (close == std::string::npos) {
                std::cerr << "[conf] Malformed section at line " << lineNo << "\n";
                continue;
            }
            section = trim(line.substr(1, close - 1));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "[conf] Skipping malformed line " << lineNo << "\n";
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = unquote(trim(line.substr(eq + 1)));

        if (section == "server") {
            if      (key == "name")        gConfig.serverName  = val;
            else if (key == "motd")        gConfig.serverMotd  = val;
            else if (key == "max_players") gConfig.maxPlayers  = std::stoi(val);
            else std::cerr << "[conf] Unknown key [server]." << key << "\n";
        }
        else if (section == "network") {
            if      (key == "bind_address") gConfig.bindAddress = val;
            else if (key == "port")         gConfig.port        = std::stoi(val);
            else std::cerr << "[conf] Unknown key [network]." << key << "\n";
        }
        else if (section == "heartbeat") {
            if      (key == "host")             gConfig.heartbeatHost            = val;
            else if (key == "path")             gConfig.heartbeatPath            = val;
            else if (key == "port")             gConfig.heartbeatPort            = std::stoi(val);
            else if (key == "public")           gConfig.heartbeatPublic          = parseBool(val);
            else if (key == "interval_minutes") gConfig.heartbeatIntervalMinutes = std::stoi(val);
            else std::cerr << "[conf] Unknown key [heartbeat]." << key << "\n";
        }
        else if (section == "save") {
            if (key == "interval_minutes") gConfig.saveIntervalMinutes = std::stoi(val);
            else std::cerr << "[conf] Unknown key [save]." << key << "\n";
        }
        else {
            std::cerr << "[conf] Unknown section [" << section << "] at line "
                      << lineNo << "\n";
        }
    }

    std::cout << "[conf] Loaded " << path << "\n";
    return true;
}