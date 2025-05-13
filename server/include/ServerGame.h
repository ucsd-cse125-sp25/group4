#pragma once
#include "ServerNetwork.h"
#include "NetworkData.h"
#include <chrono>
#include <thread>
#include <cstdint>、
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

class ServerGame {
public:
	ServerGame(void);
	~ServerGame(void);

	void update();
	void receiveFromClients();
	void sendUpdates();
	void applyMovements();
	void applyCamera();
	void updateClientPositionWithCollision(unsigned int, float, float, float);
	void applyAttacks();
	void readBoundingBoxes();


private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];

	/* Collision */
	// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	vector<vector<float>> boxes2d;
	// colors2d[i][0..3] = R, G, B, A (0–255)
	vector<vector<int>> colors2d;

	/* State */
	GameState* state;
	std::unordered_map<uint8_t, MovePayload> latestMovement;
	std::unordered_map<uint8_t, CameraPayload> latestCamera;

	/* Attack */
	std::unordered_map<unsigned, AttackPayload> latestAttacks;
	static constexpr float attackAngleDeg = 45.0f;
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
		float cosMax = cosf(DirectX::XMConvertToRadians(attackAngleDeg));
        // Initialize the static member variable
		return dot >= cosMax;                                 // within cone
	}

};