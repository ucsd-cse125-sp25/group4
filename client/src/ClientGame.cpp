#include "ClientGame.h"

ClientGame::ClientGame(void) {
	network = new ClientNetwork();

	InitPayload init{};  // empty payload for now
	char packet_data[HDR_SIZE + sizeof(InitPayload)];

	NetworkServices::buildPacket<InitPayload>(PacketType::INIT_CONNECTION, init, packet_data);

	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(InitPayload));
}

void ClientGame::sendDebugPacket(uint64_t tick) {
	DebugPayload dbg{};
	sprintf_s(dbg.message, sizeof(dbg.message), "debug: tick %llu", tick);

	char packet_data[HDR_SIZE + sizeof(DebugPayload)];
	NetworkServices::buildPacket<DebugPayload>(PacketType::DEBUG, dbg, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(DebugPayload));
}

void ClientGame::update() {
	int len = network->receivePackets(network_data);
	if (len <= 0) return;

	// here, network_data should contain the game state packet
	PacketHeader* hdr = (PacketHeader*)network_data;
	switch (hdr->type) {
	case PacketType::GAME_STATE: 
	{
		GameStatePayload* game_state = (GameStatePayload*)(network_data + HDR_SIZE);
		printf("received update for tick %llu \n", game_state->tick);
		// add logic here
		if (game_state->tick % (uint64_t)64 == 0) {
			sendDebugPacket(game_state->tick);
		}
		break;
	}
	case PacketType::DEBUG: 
	{
		break;
	}
	default:
		printf("error in packet type %d, expected GAME_STATE or DEBUG\n", hdr->type);
		break;
	}
}

ClientGame::~ClientGame() {
	delete network;
}