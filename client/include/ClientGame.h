#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include "ClientNetwork.h"
#include "NetworkData.h"

class ClientGame {
public:
	ClientGame(void);
	~ClientGame(void);

	void sendDebugPacket(uint64_t);
	void update();

private:
	ClientNetwork* network;
	char network_data[MAX_PACKET_SIZE];
};