#pragma once
#include <cstdint>
#include <cstring>

#define MAX_PACKET_SIZE 1000

enum class PacketType : uint32_t {
	INIT_CONNECTION = 0,
	DEBUG = 1,
	GAME_STATE = 2,
	MOVE = 3,
	IDENTIFICATION = 4,
	CAMERA = 5,
	// add more here
	ATTACK = 6,
	HIT = 7,
	DODGE = 8,
	DODGE_OK = 9
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

typedef struct IDPayload {
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

struct DodgePayload { float yaw, pitch; };

struct DodgeOkPayload { uint8_t invulTicks; };	// invulTicks is more like a placeholder for now

struct Packet {
	unsigned int packet_type;

	void serialize(char* data) {
		memcpy(data, this, sizeof(Packet));
	}

	void deserialize(char* data) {
		memcpy(this, data, sizeof(Packet));
	}
};

