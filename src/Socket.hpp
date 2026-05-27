#pragma once
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  using SOCKET = int;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define closesocket(s) close(s)
#endif
#include "Logger.hpp"
#include "conf.hpp"

// #define NET_SOCK_ADDR "0.0.0.0"
// #define NET_SOCK_PORT 25565

class Socket
{
private:
    Logger log;
    SOCKET mainSocket = INVALID_SOCKET;

public:
    bool running = false;

    int sockInit();
    int sockBind();
    int sockListen();
    void sockClose();
    SOCKET sockAccept();
};

inline std::string getClientIp(SOCKET s) {
    struct sockaddr_in addr = {};
    socklen_t len = sizeof(addr);
    if (getpeername(s, (struct sockaddr*)&addr, &len) != 0)
        return "unknown";
    return inet_ntoa(addr.sin_addr);
}