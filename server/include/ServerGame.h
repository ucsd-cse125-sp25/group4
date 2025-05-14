#pragma once
#include "ServerNetwork.h"
#include "NetworkData.h"
#include <chrono>
#include <thread>
#include <cstdint>、
#include <vector>
#include <unordered_map>
#include <functional>

class ServerGame {
public:
	ServerGame(void);
	~ServerGame(void);

	void update();
	void receiveFromClients();
	void sendGameStateUpdates();
	void sendAppPhaseUpdates();

	void applyMovements();
	void applyCamera();
	void updateClientPositionWithCollision(unsigned int, float, float);
	void readBoundingBoxes();
	void handleStartMenu();
	void handleShopPhase();
	void startARound(int);
	void startTimer(int, std::function<void()>);


private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;
	static int round_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];

	/* Collision */
	// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	vector<vector<float>> boxes2d;
	// colors2d[i][0..3] = R, G, B, A (0–255)
	vector<vector<int>> colors2d;

	int num_players = 4;

	/* State */
	AppState* appState;
	GameState* state;
	std::unordered_map<uint8_t, MovePayload> latestMovement;
	std::unordered_map<uint8_t, CameraPayload> latestCamera;
	// indicate whether each player is ready to move on to next phase
	std::unordered_map<uint8_t, bool> phaseStatus;
};