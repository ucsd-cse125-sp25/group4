#pragma once
#include "linalg.h"
#include "Renderer.h"
using namespace linalg::aliases;
class Player {
	float3 position;
	float3 velocity;
	float2 orientation;
	int num_coins;
};
class Runner : Player {
	bool alive;
	int health;
};
class Hunter : Player {
	int num_kills;
};

// only on the server
typedef struct {
	// CollisionShape collision_shapes[MAX_COLLISION_SHAPES]; // later we can accelerate this structure
} ServerData;

// sent from the server to the client
typedef struct {
	Runner runners[3];
	Hunter hunter;
	int hunters_score;
	int runners_score;
} ServerState;

/*
class Scene {
	StaticSceneComponent static_component; // copied to GPU once
	DynamicSceneComponent dynamic_component; // copied to GPU each frame
	Scene(string filename);
};
*/


typedef struct {
	Renderer renderer;
	/*
	// maybe store these two in a sum type?
	bool is_hunter;
	int runner_id; // in {0, 1, 2}
	*/
} ClientState;

typedef struct {
	ServerState server_state;
	ClientState client_state;
} State;
