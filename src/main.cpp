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

	running = true;
	while(running){
		SOCKET clientSocket = accept(socket.mainSocket, NULL, NULL);
		if(clientSocket == INVALID_SOCKET){
			log.err("Accept failed: " + std::to_string(WSAGetLastError()));
			continue;
		}
		log.info("Client connected");
		closesocket(clientSocket);
	}

	return 0;
}

