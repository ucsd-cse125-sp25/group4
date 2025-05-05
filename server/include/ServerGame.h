#pragma once
#include "ServerNetwork.h"
#include "NetworkData.h"
#include <chrono>
#include <thread>
#include <cstdint>、
#include <vector>

class ServerGame {
public:
	ServerGame(void);
	~ServerGame(void);

	void update();
	void receiveFromClients();
	void sendUpdates();
	void updateClientPositionWithCollision(unsigned int clientId, GameState* newState);
	void readBoundingBoxes();


private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	uint64_t tick_count = 0;
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];
	// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	vector<vector<float>> boxes2d;
	// colors2d[i][0..3] = R, G, B, A (0–255)
	vector<vector<int>> colors2d;
	GameState* state;
};