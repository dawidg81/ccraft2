#pragma once

#include "command.hpp"
#include "level.hpp"
#include "Socket.hpp"
#include <string>

extern std::string confServerName;
extern std::string confServerMotd;
extern CommandHandler cmdHandler;
extern LevelRegistry levelRegistry;
extern Socket serverSocket;