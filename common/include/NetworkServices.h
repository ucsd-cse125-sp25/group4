#pragma once
#include <winsock2.h>
#include <Windows.h>
#include "NetworkData.h"

class NetworkServices {
public:

	static int sendMessage(SOCKET curSocket, char* message, int messageSize);
	static int recvMessage(SOCKET curSocket, char* buffer, int bufSize);
	static int recvAll (SOCKET curSocket, char* buffer, int n);
	static bool checkMessage(SOCKET curSocket);

	template<typename Payload>
	static size_t buildPacket(PacketType type, const Payload& payload, char* buf) {
		// write header
		PacketHeader* hdr = (PacketHeader*)(buf);
		hdr->type = type;
		hdr->len = HDR_SIZE + (uint32_t)(sizeof(Payload));
		// write payload
		memcpy(buf + HDR_SIZE, &payload, sizeof(Payload));
		return hdr->len;
	}
};