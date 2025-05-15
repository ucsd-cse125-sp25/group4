#pragma once
#include "ServerNetwork.h"
#include "NetworkData.h"
#include <chrono>
#include <thread>
#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include <DirectXMath.h>

#define GRAVITY 0.01f
#define JUMP_VELOCITY 0.2f
#define TERMINAL_VELOCITY -0.2f
#define ATTACK_RANGE 4.0f
#define ATTACK_ANGLE_DEG 45.0f


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
	void applyPhysics();
	void updateClientPositionWithCollision(unsigned int, float, float, float);
	void applyAttacks();
	void applyDodge();
	void readBoundingBoxes();
	void handleStartMenu();
	void handleShopPhase();
	void startARound(int);

private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];

	/* Collision */
	// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	vector<BoundingBox> boxes2d;
	// colors2d[i][0..3] = R, G, B, A (0–255)
	vector<vector<int>> colors2d;

	int num_players = 4;
	int round_id;

	/* State */
	AppState* appState;
	GameState* state;
	std::unordered_map<uint8_t, MovePayload> latestMovement;
	std::unordered_map<uint8_t, CameraPayload> latestCamera;
	// indicate whether each player is ready to move on to next phase
	std::unordered_map<uint8_t, bool> phaseStatus;

	/* Attack */
	std::unordered_map<unsigned, AttackPayload> latestAttacks;
	static bool isHit(const AttackPayload& a,
		const PlayerState& victim)
	{
		// forward direction from yaw/pitch → unit vector
		float fx = cosf(a.pitch) * -sinf(a.yaw);
		float fy = cosf(a.pitch) * cosf(a.yaw);
		float fz = sinf(a.pitch);

		// vector attacker → victim
		float vx = victim.x - a.originX;
		float vy = victim.y - a.originY;
		float vz = victim.z - a.originZ;

		float dist2 = vx * vx + vy * vy + vz * vz;
		if (dist2 > a.range * a.range) return false;          // out of reach

		float len = sqrtf(dist2);
		if (len < 1e-4f) return false;                        // same spot?
		float dot = (vx * fx + vy * fy + vz * fz) / len;          // cosθ
		float cosMax = cosf(DirectX::XMConvertToRadians(ATTACK_ANGLE_DEG));
        // Initialize the static member variable
		return dot >= cosMax;                                 // within cone
	}

	// Dodge
	static constexpr uint64_t DODGE_COOLDOWN_TICKS = 120;   // 2 s  (change to 60 if desired)
	static constexpr uint8_t  INVUL_TICKS = 30;    // 0.5 s
	static constexpr float    DASH_SPEED_MULTIPLIER = 2.5f; // run speed while dashing

	std::array<uint64_t, 4> lastDodgeTick{ 0,0,0,0 };    // when each survivor last dodged
	std::array<int8_t, 4> invulTicks{ 0,0,0,0 };    // frames of invulnerability left
	std::array<int8_t, 4> dashTicks{ 0,0,0,0 };    // frames of dash‑speed left


};

static bool checkCollision(BoundingBox, BoundingBox);
static float findDistance(BoundingBox, BoundingBox, char);