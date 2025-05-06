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
		//x, y, z, yaw, pitch, speed, coins, isHunter
		.players = {
			{ 4.0f,  4.0f, 0.0f, 0.0f, 0.0f, 0.015f, 0, false},
			{-2.0f,  2.0f, 0.0f, 0.0f, 0.0f, 0.015f, 0, false},
			{ 2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.015f, 0, false},
			{-2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.015f, 0, true },
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
				printf("[CLIENT %d] MOVE_PACKET: DIR '%c', PITCH %f, YAW %f\n", id, mv->direction, mv->pitch, mv->yaw);
				// register the latest movement, but do not update yet
				latestMovement[id] = *mv;
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


		// convert intent + yaw into 2d vector
		float dx = 0, dy = 0;
		const float cy = cosf(mv.yaw);
		const float sy = sinf(mv.yaw);

		switch (mv.direction)
		{
		case 'W':  dx = cy; dy = sy; break;
		case 'S':  dx = -cy; dy = -sy; break;
		case 'A':  dx = -sy; dy = cy; break;
		case 'D':  dx = sy; dy = -cy; break;
		default: break;
		}

		dx *= state->players[id].speed;
		dy *= state->players[id].speed;

		updateClientPositionWithCollision(id, dx, dy);
	}

	latestMovement.clear(); // consume the movement, don't keep for next tick
}

void ServerGame::updateClientPositionWithCollision(unsigned int clientId, float dx, float dy) {
	// Update the position with collision detection
	float delta[3] = { dx, dy, 0.0f };
	for (int i = 0; i < 3; i++) {
		bool isColliding = false;

		// Check for collisions against other players
		for (int c = 0; c < 4; c++) {
			if (c == (int)clientId) {
				continue;
			}
			// Bounding box for the current client
			float temp = 1.0f; // radius of player
			float minBoundCurrent[2] = {
				state->players[clientId].x + delta[0] - temp, // x - 0.5
				state->players[clientId].y + delta[1] - temp  // y - 0.5
			};
			float maxBoundCurrent[2] = {
				state->players[clientId].x + delta[0] + temp, // x + 0.5
				state->players[clientId].y + delta[1] + temp  // y + 0.5
			};

			// Bounding box for the other client
			float minBoundOther[2] = {
				state->players[c].x - temp, // x - 0.5
				state->players[c].y - temp  // y - 0.5
			};
			float maxBoundOther[2] = {
				state->players[c].x + temp, // x + 0.5
				state->players[c].y + temp  // y + 0.5
			};

			// Check for overlap in both x and y directions
			isColliding = isColliding || ((minBoundCurrent[0] <= maxBoundOther[0] && maxBoundCurrent[0] >= minBoundOther[0]) &&
				(minBoundCurrent[1] <= maxBoundOther[1] && maxBoundCurrent[1] >= minBoundOther[1]));

			if (isColliding) {
				printf("Collision detected between client %d and client %d, %d\n", clientId, c, i);
				delta[i] = 0; //reset position in this direction
				break; // don't need to check other clients
			}
			else {
				//printf("no collision %d\n", i);
			}
		}

		// check for bounding box collisions
		// TODO: currently the setup prevents anything else other than
		// cube 0 to move.
		for (size_t b = 0; b < boxes2d.size(); ++b) {
			// Bounding box for the current client
			float temp = 1.0f;
			float minBoundCurrent[2] = {
				state->players[clientId].x + delta[0] - temp, // x - 0.5
				state->players[clientId].y + delta[1] - temp  // y - 0.5
			};
			float maxBoundCurrent[2] = {
				state->players[clientId].x + delta[0] + temp, // x + 0.5
				state->players[clientId].y + delta[1] + temp  // y + 0.5
			};

			auto& box = boxes2d[b];
			float staticMinX = box[0];
			float staticMinY = box[1];
			float staticMaxX = box[3];
			float staticMaxY = box[4];

			// simple AABB overlap test in 2D (X vs X, Y vs Y)
			if (minBoundCurrent[0] <= staticMaxX && maxBoundCurrent[0] >= staticMinX &&
				minBoundCurrent[1] <= staticMaxY && maxBoundCurrent[1] >= staticMinY)
			{
				// collision! cancel movement on this axis
				isColliding = true;
				printf("Collision detected between client %d and collision box %d, %d\n", clientId, b, i);
				delta[i] = 0;
				break;
			}
		}
		
	}
	state->players[clientId].x += delta[0];
	state->players[clientId].y += delta[1];
	state->players[clientId].z += delta[2];

	printf("[CLIENT %d] MOVE: %f, %f, %f\n", clientId, state->players[clientId].x, state->players[clientId].y, state->players[clientId].z);
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
		vector<float> box2d;
		// convert to Raylib coords in boxes2d
		box2d.push_back(bx0);  // min.x
		box2d.push_back(by0);  // min.y  blender Z
		box2d.push_back(bz0);  // min.z  -blender Y
		box2d.push_back(bx1);   // max.x
		box2d.push_back(by1);   // max.y
		box2d.push_back(bz1);   // max.z
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