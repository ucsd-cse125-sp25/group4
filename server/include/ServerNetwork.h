#pragma once
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <map>
#include "NetworkServices.h"
#include "NetworkData.h"

using namespace std;

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "6881"

class ServerNetwork {
public:
	ServerNetwork(void);
	~ServerNetwork(void);

	SOCKET ListenSocket;

	SOCKET ClientSocket;

	int iResult;

	std::map<unsigned int, SOCKET> sessions;

	bool acceptNewClient(unsigned int& id);
	int receiveData(unsigned int client_id, char* recvbuf);
	void sendToAll(char* packets, int totalSize);
	void sendToClient(unsigned int client_id, char* packets, int totalSize);
};