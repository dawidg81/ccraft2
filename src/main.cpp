#include <chrono>
#include <iomanip>
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN

#include <sstream>
#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <zlib.h>
#include <vector>
#include <cstring>
#include <fstream>
#include <queue>
#include <random>
#include <atomic>
#include <memory>
#include <functional>
#include <signal.h>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <netdb.h>
#endif

#include "Logger.hpp"
#include "Socket.hpp"
#include "crypt.hpp"
#include "utils.hpp"
#include "conf.hpp"
#include "level.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "command.hpp"
#include "commands.hpp"
#include "registry.hpp"
#include "conf.hpp"

#include "globals.h"

using namespace std;

Packet pack;

std::mutex playersMutex;
std::map<uint8_t, Player*> players;

bool recvExact(SOCKET socket, char* buf, int len){
	int total = 0;
	while(total < len){
		int n = recv(socket, buf + total, len - total, 0);
		if(n <= 0) return false;
		total += n;
	}
	return true;
}

string urlEncode(const string& s) {
    ostringstream out;

    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out << c;
        } else {
            out << '%'
                << uppercase
                << hex
                << setw(2)
                << setfill('0')
                << (int)c
                << nouppercase
                << dec;
        }
    }

    return out.str();
}

void handlePlayer(SOCKET clientSocket){
	Player* player = pack.recvPlayerId(clientSocket);
	if(player == nullptr) return;

	if(player->verKey != md5(serverSalt + player->username)){
		char buf[65] = {};
		buf[0] = 0x0e;
		writeMCString(buf + 1, "Login failed!");
		send(clientSocket, buf, sizeof(buf), 0);
		logger.err(player->username + " failed authentication. Expected " + md5(serverSalt + player->username) + " but got " + player->verKey);
		closesocket(clientSocket);
		delete player;
		return;
	}

	if(player->isBanned){
		char buf[65] = {};
		buf[0] = 0x0e;
		writeMCString(buf + 1, "Player '" + player->username + "' is blacklisted");
		send(clientSocket, buf, sizeof(buf), 0);
		logger.err(player->username + " tried to join but is blacklisted");
		closesocket(clientSocket);
		delete player;
		return;
	}

	{
		lock_guard<mutex> lock(playersMutex);
		for(auto& pair : players){
			if(pair.second->username == player->username){
				char buf[65] = {};
				buf[0] = 0x0e;
				writeMCString(buf + 1, "Username '" + player->username + "' already taken");
				send(clientSocket, buf, sizeof(buf), 0);
				closesocket(clientSocket);
				delete player;
				return;
			}
		}
	}

	string name = "default";
	string motd = "Welcome";
	char utype = player->isOP ? 0x64 : 0x00;
	auto stopSender = make_shared<atomic<bool>>(false);

	{
		lock_guard<mutex> lock(playersMutex);
		player->id = assignId();
		players[player->id] = player;
	}

	thread([player, clientSocket, stopSender](){
			char buf[512];
			while(!stopSender->load()){
			int n = recv(clientSocket, buf, sizeof(buf), 0);
			if(n <= 0){
			player->disconnected = true;
			break;
			}
			for(int i=0; i<n; i++) player->pushByte((uint8_t)buf[i]);
			}
			}).detach();

	/*
	   thread senderThread([player](){
	   while(true){
	   this_thread::sleep_for(chrono::milliseconds(10));
	   player->flushQueue();
	   lock_guard<mutex> lock(playersMutex);
	   if(players.find(player->id) == players.end()) break;
	   }
	   });
	   senderThread.detach();
	   */

	thread([player, stopSender](){
			while(!stopSender->load()){
			this_thread::sleep_for(chrono::milliseconds(10));
			player->flushQueue();
			}
			player->flushQueue();
			}).detach();

	pack.sendServerId(clientSocket, name, motd, utype);
	Level* startLevel = levelRegistry.getOrLoad(player->currentLevel);
	pack.sendLevel(clientSocket, *startLevel);

	{
		lock_guard<mutex> lock(playersMutex);
		for(auto& pair : players)
			pack.sendMessage(player, pair.second, "&e" + player->username + " joined the game");
	}

	player->x = (startLevel->sizeX / 2) * 32;
	player->y = (startLevel->sizeY / 2) * 32 + 51;
	player->z = (startLevel->sizeZ / 2) * 32;

	{
		lock_guard<mutex> lock(playersMutex);
		for(auto& pair : players){
			Player* other = pair.second;
			if(other->id == player->id) continue;
			if(other->currentLevel != player->currentLevel) continue;
			pack.sendSpawnPlayer(other, player);
			pack.sendSpawnPlayer(player, other);
		}
	}

	{
		char buf[10] = {};
		buf[0] = 0x08;
		buf[1] = (int8_t)-1;
		buf[2] = (player->x >> 8) & 0xFF; buf[3] = player->x & 0xFF;
		buf[4] = (player->y >> 8) & 0xFF; buf[5] = player->y & 0xFF;
		buf[6] = (player->z >> 8) & 0xFF; buf[7] = player->z & 0xFF;
		buf[8] = player->yaw;
		buf[9] = player->pitch;
		send(clientSocket, buf, sizeof(buf), 0);
	}

	auto qrecvExact = [&](char* buf, int len) -> bool {
		while(!player->disconnected){
			if(player->popExact(buf, len)) return true;
			this_thread::sleep_for(chrono::milliseconds(1));
		}
		return false;
	};

	while(true){
		if(player->disconnected) goto disconnect;
		char packetId = 0;
		if(!qrecvExact(&packetId, 1)) break;

		switch((uint8_t)packetId){
			case 0x05:{ // set block
					  char buf[7] = {};
					  if(!qrecvExact(buf, 7)) goto disconnect;
					  short bx = (short)((uint8_t)buf[0] << 8 | (uint8_t)buf[1]);
					  short by = (short)((uint8_t)buf[2] << 8 | (uint8_t)buf[3]);
					  short bz = (short)((uint8_t)buf[4] << 8 | (uint8_t)buf[5]);
					  uint8_t mode = (uint8_t)buf[6];

					  char btbuf[1] = {};
					  if(!qrecvExact(btbuf, 1)) goto disconnect;
					  uint8_t blockType = (uint8_t)btbuf[0];

					  uint8_t newBlock = (mode == 0x01) ? blockType : 0x00;
					  Level* lvl = levelRegistry.getOrLoad(player->currentLevel);
					  if(lvl) lvl->setBlock(bx, by, bz, newBlock);

					  lock_guard<mutex> lock(playersMutex);
					  for(auto& pair : players)
						  if(pair.second->currentLevel == player-> currentLevel)
							  pack.sendSetBlock(pair.second, bx, by, bz, newBlock);

					  break;
				  }
			case 0x08:{ // pos ort
					  char buf[9] = {};
					  if(!qrecvExact(buf, 9)) goto disconnect;

					  player->x = (short)((uint8_t)buf[1] << 8 | (uint8_t)buf[2]);
					  player->y = (short)((uint8_t)buf[3] << 8 | (uint8_t)buf[4]);
					  player->z = (short)((uint8_t)buf[5] << 8 | (uint8_t)buf[6]);
					  player->yaw = (uint8_t)buf[7];
					  player->pitch = (uint8_t)buf[8];

					  lock_guard<mutex> lock(playersMutex);
					  for(auto& pair: players){
						  if(pair.second->id != player->id)
							  pack.sendPositionUpdate(player, pair.second);
					  }
					  break;
				  }
			case 0x0d:{ // msg
					  char buf[65] = {};
					  if(!qrecvExact(buf, 65)) goto disconnect;

					  string msg; msg.assign(buf + 1, 64);
					  msg.erase(msg.find_last_not_of(' ') + 1);

					  if (cmdHandler.handle(player, msg)) break;

					  logger.info("<" + player->username + "> " + msg);

					  lock_guard<mutex> lock(playersMutex);
					  for(auto& pair : players)
						  pack.sendMessage(player, pair.second, player->username + ": " + msg);
					  break;
				  }
			default:
				  logger.err(player->username + " sent unknown packet 0x" + to_string((uint8_t)packetId));
				  pack.sendDisconnect(player, "Malformed data sent (0x" + to_string((uint8_t)packetId) + ")");
				  goto disconnect;
		}
	}
disconnect:
	string leftLevel = player->currentLevel;
	{
		lock_guard<mutex> lock(playersMutex);
		players.erase(player->id);
		for(auto& pair : players){
			if(pair.second->currentLevel == leftLevel)
				pack.sendDespawnPlayer(player, pair.second);
			pack.sendMessage(player, pair.second, "&e" + player->username + " left the game");
		}
	}

	levelRegistry.unloadIfEmpty(leftLevel);
	logger.info(player->username + " disconnected");
	stopSender->store(true);
	this_thread::sleep_for(chrono::milliseconds(50));
	closesocket(clientSocket);
	delete player;
}

void saveLoop(){
	while(true){
		this_thread::sleep_for(chrono::minutes(gConfig.saveIntervalMinutes));
		levelRegistry.saveAll();

		lock_guard<mutex> lock(levelRegistry.registryMutex);
		for(auto& pair : levelRegistry.levels){
			string path = "maps/" + pair.first + ".lvl";
			pair.second->save(path);
			if(pair.second->dirty){
				backupLevel(pair.first, path);
				pair.second->dirty = false;
			}
		}
	}
}

void heartbeat(){	
	while(true){
		/* const string host = gConfig.heartbeatHost;
		const string path = gConfig.heartbeatPath;
		const int port = gConfig.heartbeatPort; */

		size_t userCount;
		{
			lock_guard<mutex> lock(playersMutex);
			userCount = players.size();
		}

		string query =
			"name=" + urlEncode(gConfig.serverName) +
			"&port=" + to_string(gConfig.port) +
			"&users=" + to_string(userCount) +
			"&max=" + to_string(gConfig.maxPlayers) +
			"&salt=" + serverSalt +
			"&public=" + (gConfig.heartbeatPublic ? "true" : "false") +
			"&software=" + urlEncode(string("ccraft2 v") + VERSION);

		/* string request =
		   "GET " + path + "?" + query + " HTTP/1.0\r\n"
		   "Host: " + host + "\r\n"
		   "Connection: close\r\n"
		   "\r\n"; */

		string request =
			"GET " + gConfig.heartbeatPath + "?" + query + " HTTP/1.0\r\n"
			"Host: " + gConfig.heartbeatHost + "\r\n"
			"Connection: close\r\n"
			"\r\n";

		struct hostent* he = gethostbyname(gConfig.heartbeatHost.c_str());
		if (!he) {
			logger.err("Heartbeat: DNS resolution failed");
			this_thread::sleep_for(chrono::minutes(1));
			continue;
		}

		int s = ::socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0) {
			logger.err("Heartbeat: socket creation failed");
			this_thread::sleep_for(chrono::minutes(1));
			continue;
		}

		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port   = htons(gConfig.heartbeatPort);
		addr.sin_addr   = *(struct in_addr*)he->h_addr;

		if (::connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			logger.err("Heartbeat: connect failed");
			closesocket(s);
			this_thread::sleep_for(chrono::minutes(1));
			continue;
		}

		send(s, request.c_str(), (int)request.size(), 0);

		string response;
		char rbuf[512];
		int n;
		while((n = recv(s, rbuf, sizeof(rbuf) - 1, 0)) > 0){
			rbuf[n] = '\0';
			response += rbuf;
		}
		closesocket(s);

		auto pos = response.find("\r\n\r\n");
		if (pos != string::npos) {
    		string body = response.substr(pos + 4);
    		auto last = body.find_last_not_of(" \t\r\n");
    		if (last != string::npos)
        		body.erase(last + 1);
    		else
        		body.clear();

    		if (body.empty())
        		logger.info("Heartbeat: empty body. Full response:\n" + response);
    		else if (body.find("errors") != string::npos)
        		logger.err("Heartbeat error: " + body);
    		else
        		logger.info("Heartbeat OK: " + body);
		} else {
    		logger.info("Heartbeat: no header separator found. Raw response:\n" + response);
		}

		this_thread::sleep_for(chrono::minutes(gConfig.heartbeatIntervalMinutes));
	}
}

int main(){
	loadConfig("config.toml");
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#else
	signal(SIGFPE, SIG_IGN);
#endif
	signal(SIGINT, serverShutdown);
	signal(SIGTERM, serverShutdown);
	logger.showDebug = true;
	string splash = "ccraft2 v";
	splash.append(VERSION);
	logger.raw(splash);

#ifdef _WIN32
	CreateDirectoryA("maps", nullptr);
#else
	mkdir("maps", 0755);
#endif
	levelRegistry.getOrLoad("main", true);

	thread(saveLoop).detach();
	thread(heartbeat).detach();

	initCommands();

	// Socket socket; // is now global
	serverSocket.sockInit();
	serverSocket.sockBind();
	serverSocket.sockListen();

	serverSocket.running = true;
	while(serverSocket.running){
		SOCKET clientSocket = serverSocket.sockAccept();
		if(clientSocket == INVALID_SOCKET) continue;

		thread(handlePlayer, clientSocket).detach();
	}

	return 0;
}
