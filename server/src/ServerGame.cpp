#include "ServerGame.h"

unsigned int ServerGame::client_id;

ServerGame::ServerGame(void) {
	client_id = 0;
	network = new ServerNetwork();

	state = new GameState{ {0} };
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
				for (int i = 0; i < 4; i++) {
					state->position[i] += newState->position[i];
				}
				printf("[CLIENT %d] MOVE: %f\n", id, newState->position[1]);
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

void ServerGame::sendUpdates() {
	//GameStatePayload game_state{};  // this should probably be a variable in the servergame 
	//game_state.tick = tick_count;
	//char packet_data[HDR_SIZE + sizeof(GameStatePayload)];

	//NetworkServices::buildPacket<GameStatePayload>(PacketType::GAME_STATE, game_state, packet_data);

	char packet_data[HDR_SIZE + sizeof(GameState)];

	NetworkServices::buildPacket<GameState>(PacketType::GAME_STATE, *state, packet_data);

	network->sendToAll(packet_data, HDR_SIZE + sizeof(GameStatePayload));
}

ServerGame::~ServerGame() {
	delete network;
	delete state;
}