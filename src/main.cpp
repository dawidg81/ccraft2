#include "Logger.hpp"
#include "Socket.hpp"

#include <winsock.h>

bool running = false;

using namespace std;

Logger log;

int main() {
	log.raw("mcc v0.0.0");
	Socket socket;
	socket.winInit();
	socket.winBind();
	socket.winListen();

	return 0;
}

