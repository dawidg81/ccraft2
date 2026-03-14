#include "Socket.hpp"
#include <cstring>

sockaddr_in service;

int Socket::sockInit()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log.err("WSAStartup failed");
        return 1;
    }
#endif

    mainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mainSocket == INVALID_SOCKET) {
#ifdef _WIN32
        log.err("Error creating socket: " + std::to_string(WSAGetLastError()));
        WSACleanup();
#else
        log.err("Error creating socket");
#endif
        return 1;
    }
    log.info("Socket created");

    memset(&service, 0, sizeof(service));
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr(NET_SOCK_ADDR);
    service.sin_port = htons(NET_SOCK_PORT);
    return 0;
}

int Socket::sockBind()
{
    if (::bind(mainSocket, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
#ifdef _WIN32
        log.err("Bind failed: " + std::to_string(WSAGetLastError()));
#else
        log.err("Bind failed");
#endif
        closesocket(mainSocket);
        return 1;
    }
    return 0;
}

int Socket::sockListen()
{
    if (::listen(mainSocket, SOMAXCONN) == SOCKET_ERROR) {
#ifdef _WIN32
        log.err("Listen failed: " + std::to_string(WSAGetLastError()));
#else
        log.err("Listen failed");
#endif
        closesocket(mainSocket);
        return 1;
    }
    log.info("Listening on " + std::string(NET_SOCK_ADDR) + ":" + std::to_string(NET_SOCK_PORT));
    return 0;
}

SOCKET Socket::sockAccept()
{
    SOCKET client = ::accept(mainSocket, nullptr, nullptr);
    if (client == INVALID_SOCKET) {
#ifdef _WIN32
        log.err("Accept failed: " + std::to_string(WSAGetLastError()));
#else
        log.err("Accept failed");
#endif
        return INVALID_SOCKET;
    }
    log.info("Client connected");
    return client;
}
