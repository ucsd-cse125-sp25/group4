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

class ServerGame {
public:
	ServerGame(void);
	~ServerGame(void);

	void update();
	void receiveFromClients();
	void sendGameStateUpdates();
	void sendAppPhaseUpdates();
	void sendShopOptions(ShopOptionsPayload*);
	void sendPlayerPowerups();
	void sendActionOk(PacketType type, int ticks, int source, bool all, int id);

	void applyMovements();
	void applyCamera();
	void applyPhysics();
	void updateClientPositionWithCollision(unsigned int, float, float, float);
	void applyAttacks();
	void applyDodge();
	void readBoundingBoxes();
	void handleGamePhase();
	void handleStartMenu();
	void handleEndPhase();
	void newGame();
	void resetGamePos();

	void startARound(int);
	void handleShopPhase();
	void startShopPhase();
	void applyPowerups(uint8_t, uint8_t);
	bool anyWinners();

	void sendAnimationUpdates();
	void applyInstinct();
	void sendInstinctUpdate(uint64_t);

private:
	static constexpr int TICKS_PER_SEC = 64;
	static constexpr std::chrono::milliseconds TICK_DURATION{ 1000 / TICKS_PER_SEC };
	static unsigned int client_id;

	std::chrono::steady_clock::time_point next_tick = std::chrono::steady_clock::now();
	ServerNetwork* network;
	char network_data[MAX_PACKET_SIZE];

	int runner_time, hunter_time; // times for each of the players to start moving
	int runner_points, hunter_points; // points for each of the players
	
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

	// Player spawns for start and end phases
	Point playerSpawns[4] =
	{
		{ -2.30, 2.536, 0.913247 },
		{ -2.225, 2.536, 0.913247 },
		{ -2.15, 2.536, 0.913247 },
		{ -2.075, 2.536, 0.913247 },
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
	static constexpr uint32_t windupTicks = 25;                    // <0.5 s, matches animation
	static constexpr uint32_t cdDefaultTicks = TICKS_PER_SEC * 2;     // 2 s
	static constexpr uint32_t slowTicks = 32;                    // 0.5 s
	static constexpr float hunterSlowFactor = 0.2f;

	uint64_t hunterStartSlowdown = 0;
	uint64_t hunterEndSlowdown = 0;   // wind-up + cool-down window ends at tick

	struct DelayedAttack { AttackPayload attack; uint64_t hitTick; };
	std::optional<DelayedAttack> pendingSwing;

	static constexpr float ATTACK_DEFAULT_RANGE = 12.0f * PLAYER_SCALING_FACTOR;
	float attackRange;
	static constexpr float REDUCE_ATTACK_CD_MULTIPLIER = 0.5f;
	int attackCooldownTicks;

	static constexpr int INSTINCT_INTERVAL = 2 * TICKS_PER_SEC;
	static constexpr int INSTINCT_DURATION = 4 * TICKS_PER_SEC;

	bool isHit_(const AttackPayload& a, const PlayerState& victim);

	// Dodge
	static constexpr uint8_t DODGE_COOLDOWN_DEFAULT_TICKS = TICKS_PER_SEC * 2;   // 2 s  (change to 60 if desired)
	static constexpr uint8_t  INVUL_TICKS = TICKS_PER_SEC / 4;    // 0.25 s
	static constexpr float    DASH_SPEED_MULTIPLIER = 2.5f; // run speed while dashing
	static constexpr float    DASH_COOLDOWN_PENALTY = 0.05f; // run speed while on cooldown
	static constexpr float    REDUCE_DODGE_CD_MULTIPLIER = 0.75f; // each time powerup is bought, reduce cooldown by 0.75x

	std::array<uint64_t, 4> lastDodgeTick{ 0,0,0,0 };    // when each survivor last dodged
	std::array<int8_t, 4> invulTicks{ 0,0,0,0 };    // frames of invulnerability left
	std::array<int8_t, 4> dashTicks{ -1,-1,-1,-1 };    // frames of dash‑speed left
	std::array<float, 4> dodgeCooldownTicks{ DODGE_COOLDOWN_DEFAULT_TICKS,DODGE_COOLDOWN_DEFAULT_TICKS,DODGE_COOLDOWN_DEFAULT_TICKS,DODGE_COOLDOWN_DEFAULT_TICKS };    // cooldown of each player


	// Shop
	std::unordered_map<uint8_t, vector<Powerup>> playerPowerups;

	// Powerups
	std::unordered_map<uint8_t, float> extraJumpPowerup;
	int hasBear[4]{ 0, 0, 0, 0 };
	int bearTicks = 0;
	static constexpr int BEAR_TICKS = TICKS_PER_SEC * 10;
	static constexpr Point BEAR_POS{ 1.849596, 2.404163, 0.513342 };
	static constexpr float BEAR_SPEED_MULTIPLIER = 0.75f;
	static constexpr float BEAR_JUMP_BOOST = 1.0f * PLAYER_SCALING_FACTOR;
	static constexpr float BEAR_HITBOX = 5.0f * PLAYER_SCALING_FACTOR;
	int hunterBearStunTicks = 0;
	static constexpr int BEAR_STUN_TIME = TICKS_PER_SEC * 3;
	static constexpr float BEAR_STUN_MULTIPLIER = 0.1f;
	int roundTimeAdjustment = 0;

	// animation
	AnimationState animationState;
	
	int phantomTicks = 0;
	static constexpr int PHANTOM_TICKS = TICKS_PER_SEC * 5;
	int hasPhantom = 0;

	bool hasInstinct = false;
	uint64_t prevInstinctTickStart;
	uint64_t prevInstinctTickEnd;
};

static bool checkCollision(BoundingBox, BoundingBox);
static float findDistance(BoundingBox, BoundingBox, char);