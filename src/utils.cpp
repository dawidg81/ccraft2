#include "utils.hpp"
#include <algorithm>
#include <mutex>
#include "Logger.hpp"
#include "Socket.hpp"
#include "registry.hpp"

#include "globals.h"

// Logger logger;
// Socket serverSocket;

void writeMCString(char* buf, const std::string& str){
	memset(buf, ' ', 64);
	memcpy(buf, str.c_str(), std::min(str.size(), (size_t)64));
}

void serverShutdown(int sig){
	logger.info("Shutting down...");
	{
		lock_guard<recursive_mutex> lock(playersMutex);
		for(auto& pair : players){
			pack.sendDisconnect(pair.second, "Game stopped");
			pair.second->flushQueue();
		}
		levelRegistry.saveAll();
	}
	logger.info("Goodbye!");
	serverSocket.sockClose();
	exit(0);
}
