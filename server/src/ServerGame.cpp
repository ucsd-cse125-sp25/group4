#include "ServerGame.h"

unsigned int ServerGame::client_id;

ServerGame::ServerGame(void) {
	client_id = 0;
	network = new ServerNetwork();

	state = new GameState{
		{
			{2.0f, 2.0f, 0.0f},
			{-2.0f, 2.0f, 0.0f},
			{2.0f, -2.0f, 0.0f},
			{-2.0f, -2.0f, 0.0f}
		} };
}

void ServerGame::update() {
	auto now = std::chrono::steady_clock::now();
	if (now < next_tick) {
		std::this_thread::sleep_for(next_tick - now);
	}
	next_tick = std::chrono::steady_clock::now() + TICK_DURATION;
	++tick_count;

	if (network->acceptNewClient(client_id)) {
		printf("client %d has connected to the server (tick %llu)\n", client_id, tick_count);
		client_id++;
	}

	receiveFromClients();

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
				GameState* newState = (GameState*)&(network_data[i + HDR_SIZE]);
				updateClientPositionWithCollision(id, newState);
				printf("[CLIENT %d] MOVE: %f, %f\n", id, newState->position[id][0], newState->position[id][1]);
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

void ServerGame::updateClientPositionWithCollision(unsigned int clientId, GameState* newState) {
	// Update the position with collision detection
	float newPosition[3] = { 0 };
	for (int i = 0; i < 3; i++) {
		newPosition[i] = newState->position[clientId][i];

		bool isColliding = false;

		// Check for collisions against other players
		for (int c = 0; c < 4; c++) {
			if (c == (int)clientId) {
				continue;
			}

			float temp = 1.0f;
			// Bounding box for the current client
			float minBoundCurrent[2] = {
				state->position[clientId][0] + newPosition[0] - temp, // x - 0.5
				state->position[clientId][1] + newPosition[1] - temp  // y - 0.5
			};
			float maxBoundCurrent[2] = {
				state->position[clientId][0] + newPosition[0] + temp, // x + 0.5
				state->position[clientId][1] + newPosition[1] + temp  // y + 0.5
			};

			// Bounding box for the other client
			float minBoundOther[2] = {
				state->position[c][0] - temp, // x - 0.5
				state->position[c][1] - temp  // y - 0.5
			};
			float maxBoundOther[2] = {
				state->position[c][0] + temp, // x + 0.5
				state->position[c][1] + temp  // y + 0.5
			};

			// Check for overlap in both x and y directions
			isColliding = isColliding || ((minBoundCurrent[0] <= maxBoundOther[0] && maxBoundCurrent[0] >= minBoundOther[0]) &&
				(minBoundCurrent[1] <= maxBoundOther[1] && maxBoundCurrent[1] >= minBoundOther[1]));

			if (isColliding) {
				printf("Collision detected between client %d and client %d, %d\n", clientId, c, i);
				newPosition[i] = 0; //reset position in this direction
				break; // don't need to check other clients
			}
			else {
				//printf("no collision %d\n", i);
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		state->position[clientId][i] += newPosition[i];	
	}
	
	
}

void ServerGame::sendUpdates() {

	char packet_data[HDR_SIZE + sizeof(GameState)];

	NetworkServices::buildPacket<GameState>(PacketType::GAME_STATE, *state, packet_data);

	network->sendToAll(packet_data, HDR_SIZE + sizeof(GameState));
}

ServerGame::~ServerGame() {
	delete network;
	delete state;
}