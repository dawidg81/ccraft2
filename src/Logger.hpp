#pragma once
#include <iostream>
#include <string>

class Logger
{
public:
	bool showDebug = false;

	void raw(std::string msg);
	void info(std::string msg);
	void debug(std::string msg);
	void err(std::string msg);
};
