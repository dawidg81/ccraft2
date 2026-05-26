#pragma once
#include <string>

const std::string VERSION = "0.12.3-inconfig";

struct ServerConfig {
	std::string serverName = "Default Server";
	std::string serverMotd = "Welcome!";
	int maxPlayers = 256;

	std::string bindAddress = "0.0.0.0";
	int port = 25565;

	std::string heartbeatHost = "www.classicube.net";
	std::string heartbeatPath = "/server/heartbeat/";
	int         heartbeatPort = 80;
	bool        heartbeatPublic = true;
	int         heartbeatIntervalMinutes = 1;

	int saveIntervalMinutes = 5;
};

extern ServerConfig gConfig;

bool loadConfig(const std::string& path = "config.toml");