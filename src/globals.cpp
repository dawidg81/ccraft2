/*
Creating objects for all files to avoid linker errors
*/
#include "globals.h"

CommandHandler cmdHandler;
LevelRegistry levelRegistry;
Socket serverSocket;
std::string confServerName;
std::string confServerMotd;