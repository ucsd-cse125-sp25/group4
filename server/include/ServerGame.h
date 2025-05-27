#pragma once
#include "ServerNetwork.h"
#include "NetworkData.h"
#include "ReadData.h"
#include "Timer.h"
#include <chrono>
#include <thread>
#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include <DirectXMath.h>
#include <random>
#include <mutex>

#define GRAVITY 0.075f * PLAYER_SCALING_FACTOR
#define JUMP_VELOCITY 1.5f * PLAYER_SCALING_FACTOR
#define PLAYER_INIT_SPEED 1.0f * PLAYER_SCALING_FACTOR
#define TERMINAL_VELOCITY -9999.0f * PLAYER_SCALING_FACTOR
#define ATTACK_ANGLE_DEG 45.0f
#define RUNNER_SPAWN_PERIOD 5
#define HUNTER_SPAWN_PERIOD 10
#define JUMP_POWERUP 1.5f * PLAYER_SCALING_FACTOR

class ServerGame {
public:
	ServerGame(void);
	~ServerGame(void);

	void update();
	void receiveFromClients();
	void sendGameStateUpdates();
	void sendAppPhaseUpdates();
	void sendShopOptions(ShopOptionsPayload *data, int dest);

	void applyMovements();
	void applyCamera();
	void applyPhysics();
	void updateClientPositionWithCollision(unsigned int, float, float, float);
	void applyAttacks();
	void applyDodge();
	void readBoundingBoxes();
	void handleGamePhase();
	void handleStartMenu();
	void startARound(int);
	void handleShopPhase();
	void startShopPhase();
	void applyPowerups(uint8_t, uint8_t);
	bool anyWinners();

private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];

	int runner_time, hunter_time; // times for each of the players to start moving
	int runner_points, hunter_points; // points for each of the players
	int winningPointThreshold; // points needed to win the game
	
	Point hunterSpawn = { -1.17, 0.042, 0.068 }; // center of the carpet cross
	
	static constexpr int NUM_SPAWNS = 7;
	Point spawnPoints[NUM_SPAWNS] =
	{
		{ -2.175f, 2.307f, 0.92f },		// desk next to sun god
		{ -1.533f, 1.115f, 0.48f },		// on chair
		{ -1.162f, -1.346f, 0.14f },	// in bookshelf
		{ 2.255f, 2.054f, 0.03f },		// under bed
		{ 2.268f, 0.339f, 0.89f },		// on top of drawers/dresser
		{ -0.274f, -1.386f, 0.03f },	// floor next to the chimney thing
		{ -0.976f, 2.263f, 0.03f }		// under desk
	};

	/* Collision */
	// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	vector<BoundingBox> boxes2d;
	// colors2d[i][0..3] = R, G, B, A (0–255)
	vector<vector<int>> colors2d;

	int num_players = 4;
	int round_id;
	bool tiebreaker;

	std::random_device dev;
	std::mt19937 rng;
	std::uniform_int_distribution<std::mt19937::result_type> randomHunterPowerupGen;
	std::uniform_int_distribution<std::mt19937::result_type> randomRunnerPowerupGen;
	std::uniform_int_distribution<std::mt19937::result_type> randomSpawnLocationGen;


	/* State */
	AppState* appState;
	GameState* state;
	// synchornize access to states
	mutex state_mu;
	std::unordered_map<uint8_t, MovePayload> latestMovement;
	std::unordered_map<uint8_t, CameraPayload> latestCamera;
	// indicate whether each player is ready to move on to next phase
	std::unordered_map<uint8_t, bool> phaseStatus;
	// One timer object
	Timer* timer;

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
	static constexpr uint64_t DODGE_COOLDOWN_TICKS = TICKS_PER_SEC * 2;   // 2 s  (change to 60 if desired)
	static constexpr uint8_t  INVUL_TICKS = TICKS_PER_SEC / 4;    // 0.25 s
	static constexpr float    DASH_SPEED_MULTIPLIER = 2.5f; // run speed while dashing

	std::array<uint64_t, 4> lastDodgeTick{ 0,0,0,0 };    // when each survivor last dodged
	std::array<int8_t, 4> invulTicks{ 0,0,0,0 };    // frames of invulnerability left
	std::array<int8_t, 4> dashTicks{ 0,0,0,0 };    // frames of dash‑speed left


	// Shop
	std::unordered_map<uint8_t, vector<uint8_t>> playerPowerups;

	// Powerups
	std::unordered_map<uint8_t, float> extraJumpPowerup;
};

static bool checkCollision(BoundingBox, BoundingBox);
static float findDistance(BoundingBox, BoundingBox, char);