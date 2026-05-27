#include "level.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include "player.hpp"
#include "logger_instance.hpp"
#include "registry.hpp"

#include "globals.h"

// LevelRegistry levelRegistry;

vector<string> listLevelFiles() {
	vector<string> result;
#ifdef _WIN32
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA("maps\\*.lvl", &fd);
	if (hFind == INVALID_HANDLE_VALUE) return result;
	do {
		string fname = fd.cFileName;
		result.push_back(fname.substr(0, fname.size() - 4)); // strip .lvl
	} while (FindNextFileA(hFind, &fd));
	FindClose(hFind);
#else
	DIR* dir = opendir("maps");
	if (!dir) return result;
	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		string fname = entry->d_name;
		if (fname.size() > 4 && fname.substr(fname.size() - 4) == ".lvl")
			result.push_back(fname.substr(0, fname.size() - 4));
	}
	closedir(dir);
#endif
	return result;
}

void backupLevel(const string& name, const string& path){
	string backupDir = "maps/backups/" + name + ".lvl.d";
#ifdef _WIN32
	CreateDirectoryA("maps/backups", nullptr);
	CreateDirectoryA(backupDir.c_str(), nullptr);
#else
	mkdir("maps/backups", 0755);
	mkdir(backupDir.c_str(), 0755);
#endif

	int next = 1;
	while(true){
		ifstream check(backupDir + "/" + to_string(next) + ".lvl");
		if(!check.good()) break;
		next++;
	}

	ifstream src(path, ios::binary);
	ofstream dst(backupDir + "/" + to_string(next) + ".lvl", ios::binary);
	if(src && dst){
		dst << src.rdbuf();
		logger.info("Backup " + to_string(next) + " created for level: " + name);
	} else {
		logger.err("Failed to create backup for level: " + name);
	}
}

int getLatestBackup(const string& name) {
	string backupDir = "maps/backups/" + name + ".lvl.d";
	int n = 0;
	while(true){
		ifstream check(backupDir + "/" + to_string(n + 1) + ".lvl");
		if(!check.good()) break;
		n++;
	}
	return n;
}

void switchWorld(Player* player, const string& targetName){
	Level* targetLevel = levelRegistry.getOrLoad(targetName);
	if(!targetLevel){
		pack.sendMessage(player, player, "&cLevel '" + targetName + "' not found!");
		return;
	}

	string oldLevel = player->currentLevel;

	// despawn player for others on old level
	{
		lock_guard<recursive_mutex> lock(playersMutex);
		for(auto& pair : players){
			Player* other = pair.second;
			if(other->id == player->id) continue;
			if(other->currentLevel == oldLevel){
				pack.sendDespawnPlayer(player, other);
				pack.sendDespawnPlayer(other, player);
			}
		}
	}

	player->currentLevel = targetName;

	pack.sendLevel(player->socket, *targetLevel);

	short spawnX = (targetLevel->sizeX / 2) * 32;
	short spawnY = (targetLevel->sizeY / 2) * 32 + 51;
	short spawnZ = (targetLevel->sizeZ / 2) * 32;
	player->x = spawnX;
	player->y = spawnY;
	player->z = spawnZ;
	pack.sendTeleport(player, spawnX, spawnY, spawnZ, 0, 0);

	// others on new level for new player, new player for them
	{
		lock_guard<recursive_mutex> lock(playersMutex);
		for(auto& pair : players){
			Player* other = pair.second;
			if(other->id == player->id) continue;
			if(other->currentLevel == targetName){
				pack.sendSpawnPlayer(other, player);
				pack.sendSpawnPlayer(player, other);
			}
		}
	}

	levelRegistry.unloadIfEmpty(oldLevel);
}
