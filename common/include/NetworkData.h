#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <numbers>

#define MAX_PACKET_SIZE 1000
#define NUM_POWERUP_OPTIONS 3 // Number of options that display in the shop each round
#define ROUND_DURATION 45
#define WIN_THRESHOLD 10

#define GRAVITY 0.075f * PLAYER_SCALING_FACTOR
#define JUMP_VELOCITY 1.25f * PLAYER_SCALING_FACTOR
#define PLAYER_INIT_SPEED 0.65f * PLAYER_SCALING_FACTOR
#define HUNTER_INIT_SPEED 0.85f * PLAYER_SCALING_FACTOR
#define TERMINAL_VELOCITY -9999.0f * PLAYER_SCALING_FACTOR
#define ATTACK_ANGLE_DEG 45.0f
#define RUNNER_SPAWN_PERIOD 1
#define HUNTER_SPAWN_PERIOD 3
#define JUMP_POWERUP 0.25f * PLAYER_SCALING_FACTOR

#define PLAYER_INIT_COINS 1

constexpr float PLAYER_SCALING_FACTOR = 0.025;

constexpr float startYaw = std::numbers::pi;
constexpr float startPitch = 0.0f;

enum class PacketType : uint32_t {
	INIT_CONNECTION = 0,
	DEBUG = 1,
	GAME_STATE = 2,
	MOVE = 3,
	IDENTIFICATION = 4,
	CAMERA = 5,
	APP_PHASE = 6,
	PLAYER_READY = 7,
	ATTACK = 8,
	HIT = 9,
	DODGE = 10,
	ACTION_OK = 11,
	SHOP_INIT,			// server sends to each client the options
	SHOP_UPDATE,			// client sends what was purchased
	PLAYER_POWERUPS,		// powerup information of all players
	BEAR,
	ANIMATION_STATE,
	PHANTOM,
	INSTINCT,
	NOCTURNAL
};

// when adding powerups
// make sure to update the metadata below
// ensure the ordering too!!
enum class Powerup : uint8_t {
	// HUNTER POWERUPS
	HUNTER_POWERUPS = 0,
	H_INCREASE_SPEED,
	H_INCREASE_JUMP,
	H_INCREASE_VISION,
	H_MULTI_JUMPS,
	H_REDUCE_ATTACK_CD,
	H_INC_ATTACK_RANGE,
	H_BUNNY_HOP,
	H_PHANTOM,
	H_INCREASE_ROUND_TIME,
	H_NOCTURNAL,
	// ...
	NUM_HUNTER_POWERUPS,

	RUNNER_POWERUPS = 100,
	R_INCREASE_SPEED,
	R_INCREASE_JUMP,
	R_DECREASE_DODGE_CD,
	R_BEAR,
	R_MULTI_JUMPS,
	R_BUNNY_HOP,
	R_DODGE_NO_COLLIDE,
	// ...
	NUM_RUNNER_POWERUPS,
};

struct PowerupMetadata {
	uint8_t textureIdx = 0;
	uint8_t cost = 0;
	std::string name;
	std::wstring fileLocation;
};
//TODO
// KEEP THIS IN SAME ORDER AS ENUM
// map is sorted based on key, which is crucial for loading in correct textures
static std::map<Powerup, PowerupMetadata> PowerupInfo{
	{ Powerup::H_INCREASE_SPEED,	{0, 2, "H_SWIFTIES",	L"textures\\cards\\r_swifties.dds"} },
	{ Powerup::H_INCREASE_JUMP,		{1, 1, "H_HOPPERS",		L"textures\\cards\\r_hoppers.dds"} },
	{ Powerup::H_INCREASE_VISION,	{2, 3, "H_INSTINCT",	L"textures\\cards\\h_instinct.dds"} },
	{ Powerup::H_MULTI_JUMPS,	    {3, 3, "H_JUMPPERS",	L"textures\\cards\\r_skittish.dds"} },
	{ Powerup::H_REDUCE_ATTACK_CD,	{4, 3, "H_SNIPER",		L"textures\\cards\\h_sniper.dds"} },
	{ Powerup::H_INC_ATTACK_RANGE,	{5, 3, "H_HUSTLER",		L"textures\\cards\\h_hustler.dds"} },
	{ Powerup::H_BUNNY_HOP,			{6, 3, "H_HUSTLER",		L"textures\\cards\\r_bhopp.dds"} }, 
	{ Powerup::H_PHANTOM,			{7, 3, "H_PHANTOM",	L"textures\\cards\\h_phantom.dds"} },
	{ Powerup::H_INCREASE_ROUND_TIME,{8, 2, "H_TIMER",      L"textures\\cards\\h_timer.dds"} },
	{ Powerup::H_NOCTURNAL,			{9, 2, "H_NOCTURNAL",      L"textures\\cards\\h_nocturnal.dds"} },
	{ Powerup::R_INCREASE_SPEED,	{10, 2, "R_SWIFTIES",	L"textures\\cards\\r_swifties.dds"} },
	{ Powerup::R_INCREASE_JUMP,		{11, 1, "R_HOPPERS",		L"textures\\cards\\r_hoppers.dds"} },
	{ Powerup::R_DECREASE_DODGE_CD,	{12, 3, "R_REDBEAR",		L"textures\\cards\\r_redbear.dds"} },
	{ Powerup::R_BEAR,				{13, 5, "R_BEAR",		L"textures\\cards\\r_bear.dds"} },
	{ Powerup::R_MULTI_JUMPS,	    {14, 3, "R_JUMPPERS",	L"textures\\cards\\r_skittish.dds"} },
	{ Powerup::R_BUNNY_HOP,			{15, 3, "R_JUMPPERS",	L"textures\\cards\\r_bhopp.dds"} },
	{ Powerup::R_DODGE_NO_COLLIDE,	{16, 3, "R_DODGE_NO_COLLIDE",	L"textures\\cards\\r_clipper.dds"} },
};

// The packet header preceeds every packet
// It specifies the type so that we know how to parse
// It specifies the length so that we know how much more to read.
struct PacketHeader {
	PacketType type;
	uint32_t len;
};
static constexpr size_t HDR_SIZE = sizeof(PacketHeader);

struct Point {
	float x, y, z;
};

// Define payloads for each PacketType

struct InitPayload {};

struct DebugPayload {
	char message[128];
};

enum class GamePhase {
	START_MENU,
	GAME_PHASE,
	SHOP_PHASE,
	GAME_END,


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
	bool isBear;
	bool isPhantom;
	int jumpCounts; // for determining how many jumps can the player do in total
	int availableJumps; // how many jumps are left for the player
	bool dodgeCollide; // whether the player can collide with the boxes while dodging
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
	PlayerState players[4]; // player state
	float timerFrac; // fraction of time elapsed for timer
};

struct AppState {
	GameState* gameState;
	GamePhase gamePhase;
	uint8_t winners;
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
	uint8_t selection; // powerup selection for shop phase, ignore/0 otherwise
};

struct AppPhasePayload {
	GamePhase phase;
	uint8_t winner; // 0 nobody wins, 1 hunter wins, 2 runner wins
};

struct PlayerPowerupPayload {
	uint8_t powerupInfo[4][20];
};

struct AnimationState {
	uint8_t curAnims[4];
	bool isLoop[4];
};

struct DodgePayload { float yaw, pitch; };

enum Actions {
	GAME_STATE,
	MOVE,
	PLAYER_READY,
	ATTACK,
	BEAR_IMPACT,
	DODGE,
	SHOP_INIT,			
	SHOP_UPDATE,			
	BEAR,
	PHANTOM,
	INSTINCT,
	JUMP,
	NOCTURNAL
};

struct ActionOkPayload { 
	uint32_t packetType; 
	int endTick = 0;
	int id; // which player did the action
};

struct ShopOptionsPayload {
	uint8_t options[4][NUM_POWERUP_OPTIONS]; //hardcoded for 4 players
	uint8_t runner_score;
	uint8_t hunter_score;
};

struct BearPayload {};

struct PhantomPayload {};
struct NocturnalPayload {};

struct InstinctPayload {
	uint64_t nextInstinctEnd;
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

enum RunnerAnimation : UINT8 {
	RUNNER_ANIMATION_IDLE,
	RUNNER_ANIMATION_WALK,
	RUNNER_ANIMATION_DODGE,
	RUNNER_ANIMATION_DEAD,
	RUNNER_ANIMATION_COUNT,
};
enum HunterAnimation : UINT8 {
	HUNTER_ANIMATION_IDLE,
	HUNTER_ANIMATION_CHASE,
	HUNTER_ANIMATION_ATTACK,
	HUNTER_ANIMATION_COUNT
};
