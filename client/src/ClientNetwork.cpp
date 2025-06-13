#include "ClientNetwork.h"
#include <string>

ClientNetwork::ClientNetwork(std::string IPAddress) {
	WSADATA wsaData;

	ConnectSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL, 
					*ptr = NULL,
					hints;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		exit(1);
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//"127.0.0.1"
	iResult = getaddrinfo(IPAddress.c_str(), DEFAULT_PORT, &hints, &result);

	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		exit(1);
	}

	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			exit(1);
		}

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			printf("The server is down.. did not ocnnect");
		}
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		exit(1);
	}

	// 0 = BLOCKING SOCKET
	// 1 = NON-BLOCKING SOCKET
	u_long iMode = 0;

	iResult = ioctlsocket(ConnectSocket, FIONBIO, &iMode);
	if (iResult == SOCKET_ERROR) {
		printf("ioctlsocket failed with error :%d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		exit(1);
	}

	char value = 1;
	setsockopt(ConnectSocket, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
}

int ClientNetwork::receivePackets(char* recvbuf) {
	/*
	iResult = NetworkServices::recvMessage(ConnectSocket, recvbuf, MAX_PACKET_SIZE);
	if (iResult == 0) {
		printf("Connection closed\n");
		closesocket(ConnectSocket);
		WSACleanup();
		exit(1);
	}

	return iResult;
	*/
	if (!NetworkServices::checkMessage(ConnectSocket)) return 0;

	int r = NetworkServices::recvAll(ConnectSocket, recvbuf, HDR_SIZE);
	if (r != HDR_SIZE) return r; // error/socket closed

	PacketHeader* hdr = (PacketHeader*)recvbuf;

	r = NetworkServices::recvAll(ConnectSocket, recvbuf + HDR_SIZE, hdr->len - HDR_SIZE);
	if (r <= 0) return r;
	return hdr->len;
}

ClientNetwork::~ClientNetwork() {
	if (ConnectSocket != INVALID_SOCKET) {
		shutdown(ConnectSocket, SD_BOTH);
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}

	WSACleanup();
}