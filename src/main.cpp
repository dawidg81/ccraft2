#include "Logger.hpp"
#include "Socket.hpp"

#include <winsock2.h>
#include <string>

using namespace std;

Logger log;

class Player {
public:
	string username;
	string verKey;

	Player(string uname, string verkey){
		username = uname;
		verKey = verkey;
	}
};

int main() {
	log.raw("mcc v0.0.0");
	Socket socket;
	socket.winInit();
	socket.winBind();
	socket.winListen();

	socket.running = true;
	while(socket.running){
		SOCKET clientSocket = socket.winAccept();
		if(clientSocket == INVALID_SOCKET) continue;

		// Receive player identification
		char buffer[131] = {};
		int bytesRecv = recv(clientSocket, buffer, sizeof(buffer), 0);

		if(bytesRecv <= 0){
			log.err("No bytes received");
			closesocket(clientSocket);
			return 1;
		}
		uint8_t packID = buffer[0];
		uint8_t protVer = buffer[1];
		string username; username.assign(buffer + 2, 64);
		username.erase(username.find_first_of("\0 \t\r\n", 0, 6));
		string verKey; verKey.assign(buffer + 66, 64);
		uint8_t unused = buffer[130];

		Player player(username, verKey);
		log.info(username + " connected");

		// send server identification (using the same buffer)
		for(int i=0; i < sizeof(buffer); i++){
			buffer[i] = {};
		}
	}

	return 0;
}

