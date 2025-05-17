#pragma once
#include <cstdint>
#include <cstring>
#include <unordered_map>

#define MAX_PACKET_SIZE 1000
#define NUM_POWERUP_OPTIONS 3
#define ROUND_DURATION 25

enum class PacketType : uint32_t {
	INIT_CONNECTION = 0,
	DEBUG = 1,
	GAME_STATE = 2,
	MOVE = 3,
	IDENTIFICATION = 4,
	CAMERA = 5,
	APP_PHASE = 6,
	PLAYER_READY = 7,
	// add more here
	ATTACK = 8,
	HIT = 9,
	DODGE = 10,
	DODGE_OK = 11,
	SHOP_INIT,			// server sends to each client the options
	SHOP_UPDATE			// client sends what was purchased
};

enum class Powerup : unsigned int {
	// HUNTER POWERUPS
	HUNTER_POWERUPS = 0,
	H_INCREASE_SPEED = 0,
	H_INCREASE_JUMP,
	H_INCREASE_VISION,
	// ...

	NUM_HUNTER_POWERUPS,
	RUNNER_POWERUPS = 100,
	R_INCREASE_SPEED = 100,
	R_INCREASE_JUMP,
	// ...

	NUM_RUNNER_POWERUPS
};

static std::unordered_map<Powerup, int> PowerupCosts{
	{ Powerup::H_INCREASE_JUMP, 1 },
	{ Powerup::H_INCREASE_SPEED, 2 },
	{ Powerup::H_INCREASE_VISION, 3 },
	{ Powerup::R_INCREASE_SPEED, 1 },
};

// The packet header preceeds every packet
// It specifies the type so that we know how to parse
// It specifies the length so that we know how much more to read.
struct PacketHeader {
	PacketType type;
	uint32_t len;
};
static constexpr size_t HDR_SIZE = sizeof(PacketHeader);

// Define payloads for each PacketType

struct InitPayload {};

struct DebugPayload {
	char message[128];
};

enum class GamePhase {
	START_MENU,
	GAME_PHASE,
	SHOP_PHASE,


	NUM_SCREENS,
};

struct PlayerState {
	float x, y, z;
	float yaw, pitch;
	float zVelocity;
	float speed;
	uint8_t coins;
	bool isHunter;
	bool isDead;
	bool isGrounded; // is on the ground
};

struct EntityState { // this is for traps or placed objects
	float    x, y, z;
	bool placed;
	bool consumed;
};

struct BoundingBox {
	float minX, minY, minZ;
	float maxX, maxY, maxZ;
};

struct GameStatePayload {
	uint64_t tick;
	/* commented out for demo
	PlayerState player1, player2, player3;
	HunterState hunter;
	// could adds:
	// time remaining (chrono)
	// variable entities (traps placed)
	*/
};

struct GameState {
	uint64_t tick;
	// float position[4][3]; // x y z
	PlayerState players[4];
};

struct AppState {
	GameState* gameState;
	GamePhase gamePhase;
};

struct IDPayload {
	unsigned int id;
};

struct MovePayload {
	float direction[3];
	float yaw, pitch;
	bool jump;
};

struct CameraPayload {
	float yaw, pitch;
};

struct AttackPayload {
	float originX, originY, originZ;   // player position when swing happens
	float yaw;                         // direction of the swing
	float pitch;
	float range;                       // reach in world units (eg. 3 m)
};

struct HitPayload {
	uint8_t attackerId;
	uint8_t victimId;
};

struct PlayerReadyPayload {
	bool ready;
};

struct AppPhasePayload {
	GamePhase phase;
};

struct DodgePayload { float yaw, pitch; };

struct DodgeOkPayload { uint8_t invulTicks; };	// invulTicks is more like a placeholder for now

struct ShopOptionsPayload {
	uint8_t options[NUM_POWERUP_OPTIONS];
};

struct Packet {
	unsigned int packet_type;

	void serialize(char* data) {
		memcpy(data, this, sizeof(Packet));
	}

	void deserialize(char* data) {
		memcpy(this, data, sizeof(Packet));
	}
};

