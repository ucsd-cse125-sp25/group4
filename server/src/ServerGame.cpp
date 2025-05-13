#include "ServerGame.h"
#include "Parson.h"
#include <random>
#include <vector>
#include <iostream>
using namespace std;
unsigned int ServerGame::client_id;

ServerGame::ServerGame(void) {
	client_id = 0;
	network = new ServerNetwork();

	state = new GameState{
		.tick = 0,
		//x, y, z, yaw, pitch, speed, zVelocity, coins, isHunter, isDead, isGrounded
		.players = {
			{ 4.0f,  4.0f, 20.0f, 0.0f, 0.0f, 0.1f, 0.15f, 0, false, false, false },
			{-2.0f,  2.0f, 20.0f, 0.0f, 0.0f, -0.1f, 0.15f, 0, false, false, false },
			{ 2.0f, -2.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.15f, 0, false, false, false },
			{-2.0f, -2.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.15f, 0, true, false, false },
		}
	};

	//// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	//vector<vector<float>> boxes2d;
	//// colors2d[i][0..3] = R, G, B, A (0–255)
	//vector<vector<int>> colors2d;
}

void ServerGame::update() {
	auto now = std::chrono::steady_clock::now();
	if (now < next_tick) {
		std::this_thread::sleep_for(next_tick - now);
	}
	next_tick = std::chrono::steady_clock::now() + TICK_DURATION;
	++state->tick;

	if (network->acceptNewClient(client_id)) {
		printf("client %d has connected to the server (tick %llu)\n", client_id, state->tick);
		client_id++;
	}

	receiveFromClients();
	applyMovements();
	applyCamera();
	applyPhysics();
	applyAttacks();
	sendUpdates();
}

void ServerGame::receiveFromClients() {
	Packet packet;

	std::map<unsigned int, SOCKET>::iterator iter;

	for (auto& [id, sock] : network->sessions) {
		if (!NetworkServices::checkMessage(sock)) {
			continue;
		}
		int data_length = network->receiveData(id, network_data);
		if (data_length <= 0) {
			continue;
		}

		int i = 0;
		while (i < (unsigned int)data_length) {
			PacketHeader* hdr = (PacketHeader*) &(network_data[i]);

			switch (hdr->type) {
			case PacketType::INIT_CONNECTION:
			{
				printf("[CLIENT %d] INIT\n", id);
				char packet_data[HDR_SIZE + sizeof(IDPayload)];

				NetworkServices::buildPacket<IDPayload>(PacketType::IDENTIFICATION, { id }, packet_data);

				network->sendToClient(id, packet_data, HDR_SIZE + sizeof(IDPayload));
				break;
			}
			case PacketType::DEBUG:
			{
				DebugPayload* dbg = (DebugPayload*)&(network_data[i + HDR_SIZE]);
				printf("[CLIENT %d] DEBUG: %s\n", id, dbg->message);
				break;
			}
			case PacketType::MOVE:
			{
				MovePayload* mv = (MovePayload*)&(network_data[i + HDR_SIZE]);
				printf("[CLIENT %d] MOVE_PACKET: DIR (%f, %f, %f), PITCH %f, YAW %f, JUMP %d\n", id, mv->direction[0], mv->direction[1], mv->direction[2], mv->pitch, mv->yaw, mv->jump);
				// register the latest movement, but do not update yet
				latestMovement[id] = *mv;
				break;
			}
			case PacketType::CAMERA:
			{
				CameraPayload* cam = (CameraPayload*)&(network_data[i+HDR_SIZE]);
				printf("[CLIENT %d] CAMERA_PACKET: PITCH %f, YAW %f\n", id, cam->pitch, cam->yaw);
				latestCamera[id] = *cam;
				break;
			}
			case PacketType::ATTACK:
			{
				AttackPayload* atk = (AttackPayload*)&network_data[i + HDR_SIZE];
				latestAttacks[id] = *atk;      // overwrite if multiple swings this tick
				printf("[CLIENT %d] ATTACK at %.1f, %.1f, %.1f  yaw=%.2f  pitch=%.2f\n",
					id, atk->originX, atk->originY, atk->originZ, atk->yaw, atk->pitch);
				break;
			}

			default:
				printf("[CLIENT %d] ERR: Packet type %d\n", id, hdr->type);
				break;
			}

			i += hdr->len; // move to next packet in buffer
		}
	}

}

void ServerGame::applyMovements() {
	for (auto& [id, mv] : latestMovement) {
		//printf("[CLIENT %d] MOVE_INTENT: DIR '%c', PITCH %f, YAW %f\n", id, mv.direction, mv.pitch, mv.yaw);
		// update direction regardless of collision
		state->players[id].yaw = mv.yaw;
		state->players[id].pitch = mv.pitch;

		if (mv.jump && state->players[id].isGrounded == true)
			state->players[id].zVelocity = JUMP_VELOCITY;

		// normalize the direction vector
		float magnitude = sqrt(powf(mv.direction[0], 2) + powf(mv.direction[1], 2) + powf(mv.direction[2], 2));
		if (magnitude != 0)
			for (int i = 0; i < 3; i++)
				mv.direction[i] /= magnitude;

		// convert intent + yaw into 2d vector
		// CLOCKWISE positive
		// foward x/y, actual delta x/y
		float fx = -sinf(mv.yaw), fy = cosf(mv.yaw), fz = 1, dx = 0, dy = 0, dz = 0;
		
		dx = ((fx * mv.direction[0]) + (fy * mv.direction[1])) * state->players[id].speed;

		dy = ((fy * mv.direction[0]) - (fx * mv.direction[1])) * state->players[id].speed;

		dz = 0;

		updateClientPositionWithCollision(id, dx, dy, dz);
	}

	latestMovement.clear(); // consume the movement, don't keep for next tick
}

void ServerGame::applyCamera() {
	for (auto& [id, cam] : latestCamera) {
		state->players[id].yaw = cam.yaw;
		state->players[id].pitch = cam.pitch;
	}
	latestCamera.clear();
}

void ServerGame::applyPhysics() {
	for (int c = 0; c < 4; c++) {
		// By default, assumes the player is not on the ground
		state->players[c].isGrounded = false;

		// Processes z velocity (jumping & falling)
		updateClientPositionWithCollision(c, 0, 0, state->players[c].zVelocity);

		// Decreases player z velocity by gravity, up to terminal velocity
		state->players[c].zVelocity -= GRAVITY;
		if (state->players[c].zVelocity < TERMINAL_VELOCITY) state->players[c].zVelocity = TERMINAL_VELOCITY;
	}
}

void ServerGame::updateClientPositionWithCollision(unsigned int clientId, float dx, float dy, float dz) {
	// Update the position with collision detection
	float delta[3] = { dx, dy, dz };

	// Bounding box for the current client
	float temp = 1.0f;

	// Bounding box before player moves
	BoundingBox staticPlayerBox = {
			state->players[clientId].x - temp,
			state->players[clientId].y - temp,
			state->players[clientId].z - temp,
			state->players[clientId].x + temp,
			state->players[clientId].y + temp,
			state->players[clientId].z + temp
	};

	for (int i = 0; i < 3; i++) {
		bool isColliding = false;
		
		BoundingBox playerBox = {
			state->players[clientId].x - temp,
			state->players[clientId].y - temp,
			state->players[clientId].z - temp,
			state->players[clientId].x + temp,
			state->players[clientId].y + temp,
			state->players[clientId].z + temp
		};

		if (i == 0) {
			playerBox.minX += delta[0];
			playerBox.maxX += delta[0];
		}
		else if (i == 1) {
			playerBox.minY += delta[1];
			playerBox.maxY += delta[1];
		}
		else if (i == 2) {
			playerBox.minZ += delta[2];
			playerBox.maxZ += delta[2];
		}

		// Check for collisions against other players
		for (int c = 0; c < 4; c++) {
			// Skip current client
			if (c == (int)clientId) {
				continue;
			}

			BoundingBox otherClientBox = {
				state->players[c].x - temp,
				state->players[c].y - temp,
				state->players[c].z - temp,
				state->players[c].x + temp,
				state->players[c].y + temp,
				state->players[c].z + temp,
			};

			if (checkCollision(playerBox, otherClientBox)) {
				printf("Collision detected between client %d and client %d, %d\n", clientId, c, i);

				float distance = findDistance(staticPlayerBox, otherClientBox, i) * (delta[i] > 0 ? 1 : -1);
				if (abs(distance) < abs(delta[i])) delta[i] = distance;

				// If the z is being changed, reset z velocity and "ground" player
				if (i == 2) {
					state->players[clientId].zVelocity = 0;
					state->players[clientId].isGrounded = true;
				}
			}
		}

		// check for bounding box collisions
		// TODO: currently the setup prevents anything else other than
		// cube 0 to move.
		for (size_t b = 0; b < boxes2d.size(); ++b) {
			// simple AABB overlap test in 2D (X vs X, Y vs Y)
			if (checkCollision(playerBox, boxes2d[b]))
			{
				float distance = findDistance(staticPlayerBox, boxes2d[b], i) * (delta[i] > 0 ? 1 : -1);
				if (abs(distance) < abs(delta[i])) delta[i] = distance;

				// If the z is being changed, reset z velocity and "ground" player
				if (i == 2) {
					state->players[clientId].zVelocity = 0;
					state->players[clientId].isGrounded = true;
				}
			}
		}
		
	}
	state->players[clientId].x += delta[0];
	state->players[clientId].y += delta[1];
	state->players[clientId].z += delta[2];

	if (state->players[clientId].z < 0) {
		state->players[clientId].z = 0;
		state->players[clientId].zVelocity = 0;
		state->players[clientId].isGrounded = true;
	}

	//printf("[CLIENT %d] MOVE: %f, %f, %f\n", clientId, state->players[clientId].x, state->players[clientId].y, state->players[clientId].z);
}

// Checks whether or not two bounding boxes are colliding
static bool checkCollision(BoundingBox box1, BoundingBox box2) {
	bool isColliding = (
		(box1.minX < box2.maxX && box1.maxX > box2.minX) &&
		(box1.minY < box2.maxY && box1.maxY > box2.minY) &&
		(box1.minZ < box2.maxZ && box1.maxZ > box2.minZ)
	);
	return isColliding;
}

// Finds the distance between two bounding boxes on one axis
static float findDistance(BoundingBox box1, BoundingBox box2, char direction) {
	if (direction == 0) // X axis
		return min(abs(box1.minX - box2.maxX), abs(box1.maxX - box2.minX));
	if (direction == 1) // Y axis
		return min(abs(box1.minY - box2.maxY), abs(box1.maxY - box2.minY));
	if (direction == 2) // Z axis
		return min(abs(box1.minZ - box2.maxZ), abs(box1.maxZ - box2.minZ));
}

void ServerGame::applyAttacks()
{
	for (auto& [attackerId, atk] : latestAttacks)
	{
		for (unsigned victimId = 0; victimId < 4; ++victimId)
		{
			if (victimId == attackerId) continue;
			if (state->players[victimId].isDead) continue;

			if (isHit(atk, state->players[victimId]))
			{
				printf("[HIT] attacker %u hits victim %u (tick %llu)\n",
					attackerId, victimId, state->tick);

				// simple reward: +1 coin
				state->players[attackerId].coins++;

				// mark victim as dead
				state->players[victimId].isDead = true;
				printf("[HIT] victim %u is dead\n", victimId);

				HitPayload hp{ attackerId, victimId };
				char buf[HDR_SIZE + sizeof hp];
				NetworkServices::buildPacket(PacketType::HIT, hp, buf);
				network->sendToClient(victimId, buf, sizeof buf);

			}
		}
	}
	latestAttacks.clear();
}


void ServerGame::sendUpdates() {

	char packet_data[HDR_SIZE + sizeof(GameState)];

	NetworkServices::buildPacket<GameState>(PacketType::GAME_STATE, *state, packet_data);

	network->sendToAll(packet_data, HDR_SIZE + sizeof(GameState));
}

void ServerGame::readBoundingBoxes() {
	static std::random_device rd;                                    // seed source
	static std::mt19937       gen(rd());                             // mersenne twister engine
	static std::uniform_int_distribution<int> distRGBA(150, 255);    // for R,G,B
	const char* fileAddr = "bb#_bboxes.json";

	JSON_Value* rootVal = json_parse_file(fileAddr);
	if (!rootVal) { fprintf(stderr, "Cannot parse %s\n", fileAddr); }
	else {printf("Parsed %s\n", fileAddr);}
	JSON_Object* rootObj = json_value_get_object(rootVal);
	size_t       boxCnt = json_object_get_count(rootObj);


	for (size_t i = 0; i < boxCnt; i++) {
		const char* name = json_object_get_name(rootObj, i);
		JSON_Object* o = json_object_get_object(rootObj, name);
		JSON_Array* mn = json_object_get_array(o, "min");
		JSON_Array* mx = json_object_get_array(o, "max");

		// read raw Blender coords
		float bx0 = (float)json_array_get_number(mn, 0);
		float by0 = (float)json_array_get_number(mn, 1);
		float bz0 = (float)json_array_get_number(mn, 2);

		float bx1 = (float)json_array_get_number(mx, 0);
		float by1 = (float)json_array_get_number(mx, 1);
		float bz1 = (float)json_array_get_number(mx, 2);

		// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
		BoundingBox box2d = { bx0, by0, bz0, bx1, by1, bz1 };
		boxes2d.push_back(box2d);
		//cout << "box2d: " << box2d[0] << ", " << box2d[1] << ", " << box2d[2] << ", " << box2d[3] << ", " << box2d[4] << ", " << box2d[5] << endl;

		// colors2d[i][0..3] = R, G, B, A (0–255)
		vector<int> color2d;
		// pick a random Color in colors2d
		color2d.push_back(static_cast<unsigned char>(distRGBA(gen)));  // R
		color2d.push_back(static_cast<unsigned char>(distRGBA(gen)));  // G
		color2d.push_back(static_cast<unsigned char>(distRGBA(gen)));  // B
		color2d.push_back(200);                       // A
		colors2d.push_back(color2d);
	}
	json_value_free(rootVal);
}

ServerGame::~ServerGame() {
	delete network;
	delete state;
}