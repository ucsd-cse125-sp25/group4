#include "ServerNetwork.h"

ServerNetwork::ServerNetwork(void) {
	WSADATA wsaData;

	ListenSocket = INVALID_SOCKET;
	ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL,
					hints;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		exit(1);
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);

	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		exit(1);
	}

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		exit(1);
	}

	// 0 = BLOCKING SOCKET
	// 1 = NON-BLOCKING SOCKET
	u_long iMode = 1;

	// LISTEN SOCEKT NEEDS TO BE NON-BLOCKING
	iResult = ioctlsocket(ListenSocket, FIONBIO, &iMode);

	if (iResult == SOCKET_ERROR) {
		printf("ioctlsocket fialed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(1);
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);

	if (iResult == SOCKET_ERROR) {
		printf("bind fialed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		exit(1);
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);

	if (iResult == SOCKET_ERROR) {
		printf("ioctlsocket failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(1);
	}
}

bool ServerNetwork::acceptNewClient(unsigned int& id) {
	ClientSocket = accept(ListenSocket, NULL, NULL);

	if (ClientSocket == INVALID_SOCKET){
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK) {
			// no pending connection this tick
			return false;
		}

		printf("accept failed with error: %d\n", err);
		return false;                   // or handle as fatal
	}

	u_long iMode = 0;
	// set client socket back to blocking
	if (ioctlsocket(ClientSocket, FIONBIO, &iMode) == SOCKET_ERROR) {
		printf("ioctlsocket failed on client socket: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return false;
	}

	// disable nagle
	char value = 1;
	setsockopt(ClientSocket, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));

	sessions.insert(pair<unsigned int, SOCKET>(id, ClientSocket));
	return true;
}

int ServerNetwork::receiveData(unsigned int client_id, char* recvbuf) {
	if (sessions.find(client_id) != sessions.end()) {
		SOCKET curSocket = sessions[client_id];
		iResult = NetworkServices::recvMessage(curSocket, recvbuf, MAX_PACKET_SIZE);
		if (iResult == 0) {
			printf("Connection closed\n");
			closesocket(curSocket);
		}
		return iResult;
	}
	return 0;
}

void ServerNetwork::sendToAll(char* packets, int totalSize) {
	SOCKET curSocket;
	std::map<unsigned int, SOCKET>::iterator iter;
	int iResult;

	for (iter = sessions.begin(); iter != sessions.end(); iter++) {
		curSocket = iter->second;
		iResult = NetworkServices::sendMessage(curSocket, packets, totalSize);
		if (iResult == SOCKET_ERROR) {
			printf("sendToAll failed with error: %d\n", WSAGetLastError());
			closesocket(curSocket);
		}
	}
}

void ServerNetwork::sendToClient(unsigned int client_id, char* packets, int totalSize) {
	int iResult;
	if (sessions.find(client_id) != sessions.end()) {
		SOCKET curSocket = sessions[client_id];
		iResult = NetworkServices::sendMessage(curSocket, packets, totalSize);
		if (iResult == SOCKET_ERROR) {
			printf("sendToClient failed with error: %d\n", WSAGetLastError());
			closesocket(curSocket);
		}
	}
}

ServerNetwork::~ServerNetwork() {
	for (auto& [id, sock] : sessions) {
		if (sock != INVALID_SOCKET) {
			shutdown(sock, SD_BOTH);
			closesocket(sock);
		}
	}
	sessions.clear();

	if (ListenSocket != INVALID_SOCKET) {
		shutdown(ListenSocket, SD_BOTH);
		closesocket(ListenSocket);
		ListenSocket = INVALID_SOCKET;
	}

	if (ClientSocket != INVALID_SOCKET){
		shutdown(ClientSocket, SD_BOTH);
		closesocket(ClientSocket);
		ClientSocket = INVALID_SOCKET;
	}

	WSACleanup();
}