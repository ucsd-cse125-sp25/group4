#pragma once
#include "ServerNetwork.h"
#include "NetworkData.h"
#include <chrono>
#include <thread>
#include <cstdint>

class ServerGame {
public:
	ServerGame(void);
	~ServerGame(void);

	void update();
	void receiveFromClients();
	void sendUpdates();

private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	uint64_t tick_count = 0;
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];

	GameState* state;
};