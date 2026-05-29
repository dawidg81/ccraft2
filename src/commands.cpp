#include "command.hpp"
#include <mutex>
#include <map>
#include <vector>
#include <functional>
#include <string>
#include <cstring>
#include <algorithm>
#include "level.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "playerdb.hpp"

#include "globals.h"

using namespace std;

void initCommands(){
	//we structure the command in order: system name, usage string, short
	//description, long description
	cmdHandler.registerCommand(
			"kick",
			"/kick [player] <reason>",
			"Kick a player",
			"Force disconnects the given player from the server. Optionally provide a reason. OP only.",
			[](commandContext& ctx){
			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() < 2){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /kick [player name] <reason>");
			return;
			}

			string target = ctx.args[1];
			string reason = "";

			for(size_t i = 2; i < ctx.args.size(); i++){
			if(i > 2) reason += " ";
			reason += ctx.args[i];
			}

			if (target == ctx.sender->username) {
				pack.sendMessage(ctx.sender, ctx.sender, "&eYou can't kick yourself");
				return;
			}

			lock_guard<recursive_mutex> lock(playersMutex);
			for(auto& pair : players){
				if(pair.second->username == target){
					if(reason.length() > 0){
						pack.sendDisconnect(pair.second, "You've been kicked. Reason: " + reason);
					} else {
						pack.sendDisconnect(pair.second, "You've been kicked");
					}
					return;
				}
			}
			pack.sendMessage(ctx.sender, ctx.sender, "&ePlayer `" + target + "` has been not found!");
			});

	cmdHandler.registerCommand(
			"shutdown",
			"/shutdown",
			"Shuts down the server",
			"Stops the game disconnecting all players. OP only",
			[](commandContext& ctx){
			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() > 1){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /shutdown");
			return;
			}
			serverShutdown(0);
			});

	cmdHandler.registerCommand(
			"info",
			"/info",
			"Shows info",
			"Prints software version",
			[](commandContext& ctx){
			if(ctx.args.size() > 1){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /info");
			return;
			}

			pack.sendMessage(ctx.sender, ctx.sender, "&e = Server Info =");
			pack.sendMessage(ctx.sender, ctx.sender, "&eRunning ccraft2 v" + VERSION);
			});

	cmdHandler.registerCommand(
			"tp",
			"/tp [player]",
			"Teleport to player",
			"Teleports you to given player",
			[](commandContext& ctx){
			if(ctx.args.size() < 2){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /tp [player]");
			return;
			}

			string targetName = ctx.args[1];

			if(targetName == ctx.sender->username){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou can't teleport to yourself");
			return;
			}

			lock_guard<recursive_mutex> lock(playersMutex);
			for(auto& pair : players){
			if(pair.second->username == targetName){
				Player* target = pair.second;

				if(ctx.sender->currentLevel != target->currentLevel){
					switchWorld(ctx.sender, target->currentLevel);
				}

				pack.sendTeleport(ctx.sender, target->x, target->y, target->z, target->yaw, target->pitch);
				// pack.sendMessage(ctx.sender, ctx.sender, "&eTeleported to " + targetName);
				return;
			}
			}
			pack.sendMessage(ctx.sender, ctx.sender, "&cPlayer `" + targetName + "` not found!");
			});

	cmdHandler.registerCommand(
			"op",
			"/op [username]",
			"Make player an OP",
			"Grants OP privileges to the given player. OP only.",
			[](commandContext& ctx){

			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() < 2){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /op [username]");
			return;
			}

			string username = ctx.args[1];

			if(playerDB.setOp(username, true)){
			pack.sendMessage(ctx.sender, ctx.sender, "&e" + username + " is now an operator");

			lock_guard<recursive_mutex> lock(playersMutex);
			for(auto& pair : players){
				if(pair.second->username == username){
					pair.second->isOP = true;
					pack.sendMessage(pair.second, pair.second, "&eYou are now an operator!");
					break;
				}
			}
			} else {
				pack.sendMessage(ctx.sender, ctx.sender, "&cFailed to set OP status for " + username);
			}

			});

	cmdHandler.registerCommand(
			"deop",
			"/deop [username]",
			"Remove OP from player",
			"Removes OP privileges from the given player. OP only.",
			[](commandContext& ctx){
			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() < 2){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /deop [username]");
			return;
			}

			string username = ctx.args[1];

			if(playerDB.setOp(username, false)){
			pack.sendMessage(ctx.sender, ctx.sender, "&e" + username + " is no longer an operator");

			lock_guard<recursive_mutex> lock(playersMutex);
			for(auto& pair : players){
				if(pair.second->username == username){
					pair.second->isOP = false;
					pack.sendMessage(pair.second, pair.second, "&eYou are no longer an operator!");
					break;
				}
			}
			} else {
				pack.sendMessage(ctx.sender, ctx.sender, "&cFailed to remove OP status for " + username);
			}
			});

	cmdHandler.registerCommand(
			"ban",
			"/ban [username] <reason>",
			"Ban a player",
			"Bans the given player from the server. Optionally provide a reason. OP only.",
			[](commandContext& ctx){
			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() < 2){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /ban [username] <reason>");
			return;
			}

			string username = ctx.args[1];
			string reason = "";

			for(size_t i = 2; i < ctx.args.size(); i++){
			if(i > 2) reason += " ";
			reason += ctx.args[i];
			}

			if(playerDB.setBanned(username, true, reason)){
				pack.sendMessage(ctx.sender, ctx.sender, "&e" + username + " has been banned");

				lock_guard<recursive_mutex> lock(playersMutex);
				for(auto& pair : players){
					if(pair.second->username == username){
						string kickMsg = "You have been banned";
						if(reason.length() > 0){
							kickMsg += ". Reason: " + reason;
						}
						pack.sendDisconnect(pair.second, kickMsg);
						break;
					}
				}
			} else {
				pack.sendMessage(ctx.sender, ctx.sender, "&cFailed to ban " + username);
			}
			});

	cmdHandler.registerCommand(
			"unban",
			"/unban [username]",
			"Unban a player",
			"Removes a ban from the given player. OP only.",
			[](commandContext& ctx){
			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() < 2){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /unban [username]");
			return;
			}

			string username = ctx.args[1];

			if(playerDB.setBanned(username, false)){
			pack.sendMessage(ctx.sender, ctx.sender, "&e" + username + " has been unbanned");
			} else {
			pack.sendMessage(ctx.sender, ctx.sender, "&cFailed to unban " + username);
			}
			});


	cmdHandler.registerCommand(
			"save",
			"/save",
			"Saves level",
			"Saves current level to a file",
			[](commandContext& ctx){
			if(!ctx.sender->isOP){
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if(ctx.args.size() > 1){
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /save");
			return;
			}
			Level* lvl = levelRegistry.getOrLoad(ctx.sender->currentLevel);
			if(lvl){
			string path = "maps/" + ctx.sender->currentLevel + ".lvl";
			lvl->save(path);
			pack.sendMessage(ctx.sender, ctx.sender, "&eLevel saved to " + path);
			}
			});

	cmdHandler.registerCommand(
			"join",
			"/join [level name]",
			"Join a world",
			"Loads given level",
			[](commandContext& ctx){
			if (ctx.args.size() < 2) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /join [level name]");
			return;
			}
			string targetName = ctx.args[1];
			if (targetName == ctx.sender->currentLevel) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou are already on that level!");
			return;
			}
			switchWorld(ctx.sender, targetName);

			{
			lock_guard<recursive_mutex> lock(playersMutex);
			for (auto& pair : players) {
			pack.sendMessage(pair.second, pair.second, "&e" + ctx.sender->username + " went to &b" + targetName);
			}
			}
			});

	cmdHandler.registerCommand(
			"main",
			"/main",
			"Go to main level",
			"Sends you back to main level",
			[](commandContext& ctx){
			if (ctx.sender->currentLevel == "main") {
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou are already on the main level!");
			return;
			}
			switchWorld(ctx.sender, "main");

			{
			lock_guard<recursive_mutex> lock(playersMutex);
			for (auto& pair : players) {
			pack.sendMessage(pair.second, pair.second, "&e" + ctx.sender->username + " went to &bmain level");
			}
			}

			});

	cmdHandler.registerCommand(
			"new",
			"/new [level name]",
			"Create new world",
			"Creates new world and world file with given name. OP only.",
			[](commandContext& ctx){
			if (!ctx.sender->isOP) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if (ctx.args.size() < 2) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /new [level name]");
			return;
			}
			string name = ctx.args[1];
			string path = "maps/" + name + ".lvl";

			ifstream check(path);
			if (check.good()) {
			check.close();
			pack.sendMessage(ctx.sender, ctx.sender, "&cLevel '" + name + "' already exists!");
			return;
			}

			Level* lvl = levelRegistry.getOrLoad(name, true); // generate = true
			if (lvl)
				pack.sendMessage(ctx.sender, ctx.sender, "&eLevel '" + name + "' created!");
			else
				pack.sendMessage(ctx.sender, ctx.sender, "&cFailed to create level '" + name + "'");
			});

	cmdHandler.registerCommand(
			"del",
			"/del [level name]",
			"Delete a world",
			"Deletes a world with world file with given name. Backup files remain. OP only",
			[](commandContext& ctx){
			if (!ctx.sender->isOP) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}
			if (ctx.args.size() < 2) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eUsage: /del [level name]");
			return;
			}
			string name = ctx.args[1];
			if (name == "main") {
			pack.sendMessage(ctx.sender, ctx.sender, "&cCannot delete the main level!");
			return;
			}

			// Kick all players on that level to main first
			{
				lock_guard<recursive_mutex> lock(playersMutex);
				for (auto& pair : players) {
					if (pair.second->currentLevel == name) {
						pack.sendMessage(pair.second, pair.second, "&cThe level you were on was deleted. Sending you to main.");
					}
				}
			}
			// switchWorld must be called without playersMutex held
			vector<Player*> toMove;
			{
				lock_guard<recursive_mutex> lock(playersMutex);
				for (auto& pair : players)
					if (pair.second->currentLevel == name)
						toMove.push_back(pair.second);
			}
			for (Player* p : toMove)
				switchWorld(p, "main");

			// Unload from registry if loaded
			{
				lock_guard<mutex> lock(levelRegistry.registryMutex);
				auto it = levelRegistry.levels.find(name);
				if (it != levelRegistry.levels.end()) {
					delete it->second;
					levelRegistry.levels.erase(it);
				}
			}

			// Delete the file
			string path = "maps/" + name + ".lvl";
			if (remove(path.c_str()) == 0)
				pack.sendMessage(ctx.sender, ctx.sender, "&eLevel '" + name + "' deleted.");
			else
				pack.sendMessage(ctx.sender, ctx.sender, "&cFailed to delete level file '" + name + "'");
			});

	cmdHandler.registerCommand(
			"wlist",
			"/wlist",
			"Lists worlds",
			"Gives a list of available worlds to join",
			[](commandContext& ctx){
			vector<string> worlds = listLevelFiles();
			if (worlds.empty()) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eNo levels found in maps/");
			return;
			}
			pack.sendMessage(ctx.sender, ctx.sender, "&e = Available Levels =");
			for (const string& w : worlds) {
			string line = "&e  " + w;
			if (w == ctx.sender->currentLevel) line += " &a(you are here)";
			pack.sendMessage(ctx.sender, ctx.sender, line);
			}
			});

	// BACKUP CMDS START

	cmdHandler.registerCommand(
			"backtp",
			"/backtp [level name] <backup number>",
			"Browse through backup files",
			"Brings player to a backup file with given number from given level. If backup number not given, uses last backup file. At least level name needed.",
			[](commandContext& ctx){
			string levelName = ctx.sender->currentLevel;
			int backupNum = -1; // -1 = latest

			if(ctx.args.size() >= 2) levelName = ctx.args[1];
			if(ctx.args.size() >= 3) {
			try { backupNum = stoi(ctx.args[2]); }
			catch(...) {
			pack.sendMessage(ctx.sender, ctx.sender, "&cInvalid backup number!");
			return;
			}
			}

			if(backupNum == -1) backupNum = getLatestBackup(levelName);
			if(backupNum == 0) {
			pack.sendMessage(ctx.sender, ctx.sender, "&cNo backups found for level '" + levelName + "'");
			return;
			}

			string backupPath = "maps/backups/" + levelName + ".lvl.d/" + to_string(backupNum) + ".lvl";
			ifstream check(backupPath);
			if(!check.good()) {
				pack.sendMessage(ctx.sender, ctx.sender, "&cBackup " + to_string(backupNum) + " not found for level '" + levelName + "'");
				return;
			}
			check.close();

			// load backup into a temporary level and send it to just this player
			Level tmp(256, 64, 256, 0);
			tmp.load(backupPath);
			pack.sendLevel(ctx.sender->socket, tmp);

			short spawnX = (tmp.sizeX / 2) * 32;
			short spawnY = (tmp.sizeY / 2) * 32 + 51;
			short spawnZ = (tmp.sizeZ / 2) * 32;
			ctx.sender->x = spawnX;
			ctx.sender->y = spawnY;
			ctx.sender->z = spawnZ;
			pack.sendTeleport(ctx.sender, spawnX, spawnY, spawnZ, 0, 0);

			pack.sendMessage(ctx.sender, ctx.sender, "&eViewing backup " + to_string(backupNum) + " of '" + levelName + "' (read-only view)");
			});

	cmdHandler.registerCommand(
			"revert",
			"/revert <level name> <backup number>",
			"Revert world to backup",
			"Reverts world file with given level name to a backup file of given backup number. If level name and backup number not given, shows number of saved backups of current level. If backup number not given, reverts given level to last backup. Backup numbers don't change. OP only.",
			[](commandContext& ctx){
			if(!ctx.sender->isOP) {
			pack.sendMessage(ctx.sender, ctx.sender, "&eYou're not an op!");
			return;
			}

			string levelName = ctx.sender->currentLevel;
			int backupNum = -1;

			if(ctx.args.size() >= 2) levelName = ctx.args[1];
			if(ctx.args.size() >= 3) {
			try { backupNum = stoi(ctx.args[2]); }
			catch(...) {
			pack.sendMessage(ctx.sender, ctx.sender, "&cInvalid backup number!");
			return;
			}
			}

			int latest = getLatestBackup(levelName);

			// if only level name given (or no args), just print backup count
			if(ctx.args.size() <= 2 && ctx.args.size() >= 1) {
				if(latest == 0)
					pack.sendMessage(ctx.sender, ctx.sender, "&eNo backups found for level '" + levelName + "'");
				else
					pack.sendMessage(ctx.sender, ctx.sender, "&eLevel '" + levelName + "' has " + to_string(latest) + " backup(s)");
				return;
			}

			if(backupNum == -1) backupNum = latest;
			if(backupNum == 0) {
				pack.sendMessage(ctx.sender, ctx.sender, "&cNo backups found for level '" + levelName + "'");
				return;
			}

			string backupPath = "maps/backups/" + levelName + ".lvl.d/" + to_string(backupNum) + ".lvl";
			ifstream check(backupPath);
			if(!check.good()) {
				pack.sendMessage(ctx.sender, ctx.sender, "&cBackup " + to_string(backupNum) + " not found for level '" + levelName + "'");
				return;
			}
			check.close();

			Level* lvl = levelRegistry.getOrLoad(levelName);
			if(!lvl) {
				pack.sendMessage(ctx.sender, ctx.sender, "&cLevel '" + levelName + "' is not loaded!");
				return;
			}

			lvl->load(backupPath);

			// save as new version (this also creates a backup of the reverted state)
			int next = latest + 1;
			string newPath = "maps/" + levelName + ".lvl";
			lvl->save(newPath);

			// resend level to all players on it
			{
				lock_guard<recursive_mutex> lock(playersMutex);
				for(auto& pair : players) {
					if(pair.second->currentLevel == levelName) {
						pack.sendLevel(pair.second->socket, *lvl);
						short spawnX = (lvl->sizeX / 2) * 32;
						short spawnY = (lvl->sizeY / 2) * 32 + 51;
						short spawnZ = (lvl->sizeZ / 2) * 32;
						pair.second->x = spawnX;
						pair.second->y = spawnY;
						pair.second->z = spawnZ;
						pack.sendTeleport(pair.second, spawnX, spawnY, spawnZ, 0, 0);
					}
				}
			}

			pack.sendMessage(ctx.sender, ctx.sender, "&eLevel '" + levelName + "' reverted to backup " + to_string(backupNum) + " (saved as backup " + to_string(next) + ")");
			});

	cmdHandler.registerHelp();
}
