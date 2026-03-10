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
	bool isOP;

	Player(string uname, string verkey, bool op){
		username = uname;
		verKey = verkey;
		isOP = op;
	}
};

auto writeMCString = [](char* buf, const string& str){
	memset(buf, ' ', 64);
	memcpy(buf, str.c_str(), min(str.size(), (size_t)64));
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
			continue;
		}
		uint8_t packID = buffer[0];
		uint8_t protVer = buffer[1];
		string username; username.assign(buffer + 2, 64);
		username.erase(username.find_first_of("\0 \t\r\n", 0, 6));
		string verKey; verKey.assign(buffer + 66, 64);
		uint8_t unused = buffer[130];

		Player player(username, verKey, false);

		log.info(username + " connected");

		// send server identification (using the same buffer)
		for(int i=0; i < sizeof(buffer); i++){
			buffer[i] = {};
		}

		struct server_id_pack {
			uint8_t packID;
			uint8_t protVer;
			string name;
			string motd;
			uint8_t utype;
		} serverIDPack;

		serverIDPack.packID = 0x00;
		serverIDPack.protVer = 0x07;
		serverIDPack.name = "MCC Testing";
		serverIDPack.motd = "Hello, " + player.username + "!";
		if(player.isOP == true) serverIDPack.utype = 0x64;
		else serverIDPack.utype = 0x00;

		buffer[0] = serverIDPack.packID;
		buffer[1] = serverIDPack.protVer;
		writeMCString(buffer + 2, serverIDPack.name);
		writeMCString(buffer + 66, serverIDPack.motd);
		buffer[130] = serverIDPack.utype;

		int bytesSent = send(clientSocket, buffer, sizeof(buffer), 0);
		if(bytesSent != sizeof(buffer)){
			log.err("Failed to send buffer");
			closesocket(clientSocket);
			continue;
		}
	}

	return 0;
}

