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
	float speed;
	uint8_t coins;
	bool isHunter;
//	bool dead;
};

struct EntityState { // this is for traps or placed objects
	float    x, y, z;
	bool placed;
	bool consumed;
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
};

struct CameraPayload {
	float yaw, pitch;
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

