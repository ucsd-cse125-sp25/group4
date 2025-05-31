#include <random>
#include <vector>
#include <iostream>
#include <numeric>
#include "ServerGame.h"
#include "Parson.h"


using namespace std;
unsigned int ServerGame::client_id;

ServerGame::ServerGame(void) :
	rng(dev()),
	randomSpawnLocationGen(0, (unsigned int)NUM_SPAWNS - 1)
{
	client_id = 0;
	network = new ServerNetwork();
	round_id = 0;

	state = new GameState{
		.tick = 0,
		//x, y, z, yaw, pitch, zVelocity, speed, coins, isHunter, isDead, isGrounded
		.players = {
			{ 4.0f * PLAYER_SCALING_FACTOR,  4.0f * PLAYER_SCALING_FACTOR, -200.0f * PLAYER_SCALING_FACTOR, 0.0f, 0.0f, 0.0f, PLAYER_INIT_SPEED, PLAYER_INIT_COINS, true, false, false },
			{-2.0f * PLAYER_SCALING_FACTOR,  2.0f * PLAYER_SCALING_FACTOR, -200.0f * PLAYER_SCALING_FACTOR, 0.0f, 0.0f, 0.0f, PLAYER_INIT_SPEED, PLAYER_INIT_COINS, false, false, false },
			{ 2.0f * PLAYER_SCALING_FACTOR, -2.0f * PLAYER_SCALING_FACTOR, -200.0f * PLAYER_SCALING_FACTOR, 0.0f, 0.0f, 0.0f, PLAYER_INIT_SPEED, PLAYER_INIT_COINS, false, false, false },
			{-2.0f * PLAYER_SCALING_FACTOR, -2.0f * PLAYER_SCALING_FACTOR, -200.0f * PLAYER_SCALING_FACTOR, 0.0f, 0.0f, 0.0f, PLAYER_INIT_SPEED, PLAYER_INIT_COINS, false, false, false },
		},
		.timerFrac = 0.0f,
	};

	appState = new AppState{
		.gameState = state,
		.gamePhase = GamePhase::START_MENU
	};

	timer = new Timer();
	tiebreaker = false;

	//// each box → 6 floats: {min.x, min.y, min.z, max.x, max.y, max.z}
	//vector<vector<float>> boxes2d;
	//// colors2d[i][0..3] = R, G, B, A (0–255)
	//vector<vector<int>> colors2d;


	// TODO: test RNG on the atkinson hall computers
	//cout << "Entropy: " << dev.entropy() << endl;
	//for (int i = 0; i < 100; i++) {
	//	cout << randomHunterPowerupGen(rng) << endl;
	//}
}

void ServerGame::update() {
	auto now = std::chrono::steady_clock::now();
	if (now < next_tick) {
		std::this_thread::sleep_for(next_tick - now);
	}
	else {
		printf("[WARNING] Tick %llu time surpassed expected time\n", state->tick);
	}
	next_tick = std::chrono::steady_clock::now() + TICK_DURATION;
	++state->tick;

	if (network->acceptNewClient(client_id)) {
		printf("client %d has connected to the server (tick %llu)\n", client_id, state->tick);
		client_id++;
	}

	receiveFromClients();

	state_mu.lock();
	switch (appState->gamePhase) {
		case GamePhase::GAME_PHASE:
		{
			applyMovements();
			applyCamera();
			applyPhysics();
			applyAttacks();
			applyDodge();
			sendGameStateUpdates();
			handleGamePhase();
			break;
		}
		case GamePhase::START_MENU:
		{
			handleStartMenu();
			break;
		}
		case GamePhase::SHOP_PHASE:
		{
			handleShopPhase();
			break;
		}
		case GamePhase::GAME_END:
		{
			handleEndPhase();
			break;
		}
		default:
		{
			break;
		}
	}
	state_mu.unlock();
}

void ServerGame::receiveFromClients() 
{
	std::map<unsigned int, SOCKET>::iterator iter;

	for (auto& [id, sock] : network->sessions) {
		if (!NetworkServices::checkMessage(sock)) {
			continue;
		}
		int data_length = network->receiveData(id, network_data);
		if (data_length <= 0) {
			continue;
		}

		unsigned int i = 0;
		while (i < (unsigned int)data_length) {
			PacketHeader* hdr = (PacketHeader*) &(network_data[i]);

			switch (hdr->type) {
			case PacketType::INIT_CONNECTION:
			{
				printf("[CLIENT %d] INIT\n", id);
				char packet_data[HDR_SIZE + sizeof(IDPayload)];

				NetworkServices::buildPacket<IDPayload>(PacketType::IDENTIFICATION, { id }, packet_data);

				network->sendToClient(id, packet_data, HDR_SIZE + sizeof(IDPayload));

				state_mu.lock();
				phaseStatus[id] = false;
				state_mu.unlock();

				state->players[id].x = playerSpawns[id].x;
				state->players[id].y = playerSpawns[id].y;
				state->players[id].z = playerSpawns[id].z;
				state->players[id].yaw = startYaw;
				state->players[id].pitch = startPitch;

				sendGameStateUpdates();

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
				// register the latest movement, but do not update yet
				if ((id == 0 && state->tick > hunter_time)
					|| (id != 0 && state->tick > runner_time))
				{
					//printf("[CLIENT %d] MOVE_PACKET: DIR (%f, %f, %f), PITCH %f, YAW %f, JUMP %d\n", id, mv->direction[0], mv->direction[1], mv->direction[2], mv->pitch, mv->yaw, mv->jump);
 					latestMovement[id] = *mv;
				}
				break;
			}
			case PacketType::CAMERA:
			{
				CameraPayload* cam = (CameraPayload*)&(network_data[i+HDR_SIZE]);
				// printf("[CLIENT %d] CAMERA_PACKET: PITCH %f, YAW %f\n", id, cam->pitch, cam->yaw);
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
			case PacketType::DODGE:
			{
				// hunters cannot dodge
				if (state->players[id].isHunter || state->players[id].isDead) break;

				bool offCooldown = (state->tick - lastDodgeTick[id]) >= dodgeCooldownTicks[id];
				if (!offCooldown) break;                           // silently ignore spam

				// grant!
				lastDodgeTick[id] = state->tick;
				invulTicks[id] = INVUL_TICKS;
				dashTicks[id] = INVUL_TICKS;                   // dash lasts same 30 ticks

				// speed boost
				state->players[id].speed *= DASH_SPEED_MULTIPLIER;

				// notify the client
				DodgeOkPayload ok{ INVUL_TICKS };
				char buf[HDR_SIZE + sizeof ok];
				NetworkServices::buildPacket(PacketType::DODGE_OK, ok, buf);
				network->sendToClient(id, buf, sizeof buf);

				printf("[DODGE] survivor %u granted at tick %llu\n", id, state->tick);
				break;
			}

			case PacketType::PLAYER_READY:
			{
				PlayerReadyPayload* status = (PlayerReadyPayload*)&(network_data[i + HDR_SIZE]);
				printf("[CLIENT %d] PLAYER_READY_PACKET: READY=%d\n", id, status->ready);

				// Save powerup selections
				if (appState->gamePhase == GamePhase::SHOP_PHASE)
				{
					printf("Selection: %d\n", status->selection);
					// Only save if they selected a powerup
					if (status->selection != 0) 
					{
						applyPowerups(id, status->selection);
						state->players[id].coins -= PowerupInfo[(Powerup)status->selection].cost;
					}
				}
				
				state_mu.lock();
				phaseStatus[id] = status->ready;
				state_mu.unlock();

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


// Start a round
// int seconds: length of round
void ServerGame::startARound(int seconds) {
	round_id++;
	for (auto [id,powerups] : playerPowerups) {
		printf("Player %d Powerups: ", id);
		for (auto p : powerups) {
			printf("%s, ", PowerupInfo[p].name.c_str());
		}
		printf("\n");
	}
	for (unsigned int id = 0; id < num_players; ++id) {
		auto& player = state->players[id];

		// set spawn locations: center for hunter, random spots for runners
		if (player.isHunter) {
			player.x = hunterSpawn.x;
			player.y = hunterSpawn.y;
			player.z = hunterSpawn.z;
		}
		else
		{
			int spawn = randomSpawnLocationGen(rng);
			player.x = spawnPoints[spawn].x;
			player.y = spawnPoints[spawn].y;
			player.z = spawnPoints[spawn].z;
			
			float jiggle = 3 * PLAYER_SCALING_FACTOR;
			if (id == 2) {
				player.x += jiggle;
			}
			else if (id == 3) {
				player.y += jiggle;
			}
		}

		player.isDead = false;
		// print player coin
		printf("[round %d] Player %d coins: %d\n", round_id, id, player.coins);
	}
	int start_tick = state->tick;
	runner_time = start_tick + (RUNNER_SPAWN_PERIOD * TICKS_PER_SEC);
	hunter_time = start_tick + (HUNTER_SPAWN_PERIOD * TICKS_PER_SEC);

	timer->startTimer(seconds, [this]() {
		// This code runs after the timer completes
		// check if it is a tiebreaker round
		if (tiebreaker) {
			// the points will never be the same for both teams.
			if (runner_points > hunter_points) {
				printf("[round %d] Tiebreaker round ended, survivors win!\n", round_id);
			}
			else {
				printf("[round %d] Tiebreaker round ended, hunter wins!\n", round_id);
			}
			tiebreaker = false; // reset tiebreaker
			return;
		}
		
		state_mu.lock();

		// set all status to true here, handle in the main game loop
		for (auto& [id, status] : phaseStatus) {
			status = true;
		}

		// Count how many survivors survived the round
		unsigned int num_survivors = 0;
		unsigned int hunter_id = 0;
		for (unsigned int id = 0; id < num_players; ++id) {
			if (!state->players[id].isDead && !state->players[id].isHunter) {
				num_survivors++;
			}
			if (state->players[id].isHunter) {
				hunter_id = id; // save hunter id
			}
		}
		// Determine who wins this round
		if (num_survivors == 0) {
			printf("[round %d] No survivors survived the round, hunter wins!\n", round_id);
		}
		else {
			printf("[round %d] %d survivors survived the round!\n", round_id, num_survivors);
		}

		// Add points to survivors and hunter
		runner_points += num_survivors;
		hunter_points += 3 - num_survivors; // hunter gets 1 points for each survivor dead
		printf("[round %d] Runner points: %d, Hunter points: %d\n", round_id, runner_points, hunter_points);

		// Survivors each get ${3-sum_survivors} coins, Hunter gets ${sum_survivors}.
		for (unsigned int id = 0; id < num_players; ++id) {
			if (!state->players[id].isHunter) {
				state->players[id].coins += 3 - num_survivors;
				printf("[round %d] Player %d coins: %d\n", round_id, id, state->players[id].coins);
			}
			else {
				state->players[id].coins += num_survivors; // hunter gets more coins if more survivors are alive
				//printf("adding %d coins to hunter %d\n", 3 - num_survivors, id);
				printf("[round %d] Hunter %d coins: %d\n", round_id, id, state->players[id].coins);
			}
		}

		// Don't send packets in timer!
		// Socket wrapper isn't thread safe...
		// sendAppPhaseUpdates();
		state_mu.unlock();
	});
}

void ServerGame::handleStartMenu() {
	bool ready = true;
	for (auto& [id, status] : phaseStatus) {
		if (!status) {
			ready = false;
		}
	}

	if (ready && !phaseStatus.empty()) {
		// START A ROUND
		num_players = phaseStatus.size();
		appState->gamePhase = GamePhase::GAME_PHASE;
		sendAppPhaseUpdates();
		startARound(ROUND_DURATION);
		// reset status
		for (auto& [id, status] : phaseStatus) {
			status = false;
		}
	}
}

void ServerGame::newGame()
{
	round_id = 0;
	tiebreaker = false;
	playerPowerups.clear();
	runner_points = 0;
	hunter_points = 0;
	for (int i = 0; i < num_players; i++) {
		state->players[i].coins = PLAYER_INIT_COINS;
		state->players[i].speed = PLAYER_INIT_SPEED;
		state->players[i].isDead = false;
	}
}

void ServerGame::resetGamePos()
{
	for (int i = 0; i < num_players; i++)
	{
		state->players[i].x = playerSpawns[i].x;
		state->players[i].y = playerSpawns[i].y;
		state->players[i].z = playerSpawns[i].z;
		state->players[i].yaw = startYaw;
		state->players[i].pitch = startPitch;
	}

	sendGameStateUpdates();
}

void ServerGame::handleEndPhase() {
	bool ready = true;
	for (auto& [id, status] : phaseStatus) {
		if (!status) {
			ready = false;
		}
	}


	if (ready && !phaseStatus.empty()) {
		newGame();
		appState->gamePhase = GamePhase::START_MENU;
		sendAppPhaseUpdates();
		// reset status
		for (auto& [id, status] : phaseStatus) {
			status = false;
		}
	}
	else {
		resetGamePos();
	}
}

// -----------------------------------------------------------------------------
// GAME DEV HELPING FUNCTION
// -----------------------------------------------------------------------------
bool ServerGame::anyWinners() {
	return (runner_points >= WIN_THRESHOLD || hunter_points >= WIN_THRESHOLD);
}

// -----------------------------------------------------------------------------
// GAME PHASE PHASE LOGIC
// -----------------------------------------------------------------------------

void ServerGame::handleGamePhase() {
	state->timerFrac = timer->getFracElapsed();
	bool ready = true;
	for (auto& [id, status] : phaseStatus) {
		if (!status) {
			ready = false;
		}
	}

	if (ready && !phaseStatus.empty()) {
		// Check if anyone won the game	
		if (anyWinners()) {
			if (runner_points == hunter_points) {
				printf("[round %d] Game over! It's a tie! Starting a tiebreaker round. \n", round_id);
				tiebreaker = true;
				appState->gamePhase = GamePhase::SHOP_PHASE;
				startShopPhase();
			}
			else {
				printf("[round %d] Game over! Winners: %s\n", round_id, (runner_points >= WIN_THRESHOLD) ? "survivors" : "hunter");
				appState->gamePhase = GamePhase::GAME_END;
				sendAppPhaseUpdates();
			}
		}
		else
		{
			appState->gamePhase = GamePhase::SHOP_PHASE;
			startShopPhase();
		}

		for (auto& [id, status] : phaseStatus) {
			status = false;
		}
	}
}

// -----------------------------------------------------------------------------
// SHOP PHASE
// -----------------------------------------------------------------------------

void ServerGame::handleShopPhase() {
	bool ready = true;
	for (auto& [id, status] : phaseStatus) {
		if (!status) {
			ready = false;
		}
	}

	if (ready && !phaseStatus.empty()) {
		appState->gamePhase = GamePhase::GAME_PHASE;
		sendAppPhaseUpdates();
		startARound(ROUND_DURATION);
		// reset status
		for (auto& [id, status] : phaseStatus) {
			status = false;
		}
	}
}

void ServerGame::startShopPhase() {
	// send each client their powerups
	for (int id = 0; id < num_players; id++) {
		ShopOptionsPayload* options = new ShopOptionsPayload();
		if (state->players[id].isHunter)
		{
			std::vector<int> v((int)Powerup::NUM_HUNTER_POWERUPS - (int)Powerup::HUNTER_POWERUPS - 1);
			std::iota(v.begin(), v.end(), (int)Powerup::HUNTER_POWERUPS + 1);
			std::shuffle(v.begin(), v.end(), rng);
			for (int p = 0; p < NUM_POWERUP_OPTIONS; p++) {
				options->options[p] = (uint8_t) v[p];
				printf("Hunter option %d: %d %s\n", p+1, v[p], PowerupInfo[(Powerup)v[p]].name.c_str());
			}
		}
		else
		{
			std::vector<int> v((int)Powerup::NUM_RUNNER_POWERUPS - (int)Powerup::RUNNER_POWERUPS - 1);
			std::iota(v.begin(), v.end(), (int)Powerup::RUNNER_POWERUPS + 1);
			std::shuffle(v.begin(), v.end(), rng);
			for (int p = 0; p < NUM_POWERUP_OPTIONS; p++) {
				options->options[p] = (uint8_t) v[p];
				printf("Runner %d option %d: %d %s\n", id, p+1, v[p], PowerupInfo[(Powerup)v[p]].name.c_str());

			}
		}
		options->runner_score = runner_points;
		options->hunter_score = hunter_points;
		sendShopOptions(options, id);
	}
}

void ServerGame::applyPowerups(uint8_t id, uint8_t selection)
{
	playerPowerups[id].push_back(static_cast<Powerup>(selection));
	// TODO: add more powerups here
	switch ((Powerup)selection) {
	case Powerup::H_INCREASE_SPEED:
		state->players[id].speed *= 1.5f;
		printf("[POWERUP] Player %d speed increased to %.2f\n", id, state->players[id].speed);
		break;
	case Powerup::H_INCREASE_JUMP:
		extraJumpPowerup[id] += JUMP_POWERUP; // increase jump height
		printf("[POWERUP] Player %d jump height increased by %.2f\n", id, extraJumpPowerup[id]);
		break;
	case Powerup::H_INCREASE_VISION:
		printf("[POWERUP] Player %d increase vision (place holder)", id);
		break;
	case Powerup::R_INCREASE_SPEED:
		state->players[id].speed *= 1.5f;
		printf("[POWERUP] Player %d speed increased to %.2f\n", id, state->players[id].speed);
		break;
	case Powerup::R_INCREASE_JUMP:
		extraJumpPowerup[id] += JUMP_POWERUP; // increase jump height
		printf("[POWERUP] Player %d jump height increased by %.2f\n", id, extraJumpPowerup[id]);
		break;
	case Powerup::R_DECREASE_DODGE_CD:
		dodgeCooldownTicks[id] *= REDUCE_DODGE_CD_MULTIPLIER;
		printf("[POWERUP] Player %d dodge cooldown reduced to %.2f\n", id, dodgeCooldownTicks[id]);
		break;
	default:
		break;
	}
}

// -----------------------------------------------------------------------------
// GAME PHASE
// -----------------------------------------------------------------------------

void ServerGame::applyMovements() {
	for (unsigned int id = 0; id < num_players; ++id) {
		auto& player = state->players[id];
		// printf("[CLIENT %d] isGrounded=%d z=%f zVelocity=%f\n", id, player.isGrounded ? 1 : 0, player.z, player.zVelocity);

		float dx = 0, dy = 0;
		if (latestMovement.count(id)) {
			auto& mv = latestMovement[id];
			// update direction regardless of collision
			player.yaw = mv.yaw;
			player.pitch = mv.pitch;

			// normalize the direction vector
			float magnitude = sqrt(powf(mv.direction[0], 2) + powf(mv.direction[1], 2) + powf(mv.direction[2], 2));
			if (magnitude != 0)
				for (int i = 0; i < 3; i++)
					mv.direction[i] /= magnitude;

			// convert intent + yaw into 2d vector
			// CLOCKWISE positive
			// foward x/y, actual delta x/y
			float fx = -sinf(mv.yaw), fy = cosf(mv.yaw), fz = 1;

			dx = ((fx * mv.direction[0]) + (fy * mv.direction[1])) * player.speed;

			dy = ((fy * mv.direction[0]) - (fx * mv.direction[1])) * player.speed;
		}

		/* physics logic embedded */
		bool wasGrounded = player.isGrounded;
		player.isGrounded = false;

		if (latestMovement.count(id) && latestMovement[id].jump && wasGrounded == true) {
			player.zVelocity = JUMP_VELOCITY + extraJumpPowerup[id];
			printf("[CLIENT %d] Jump registered. zVelocity=%f\n", id, state->players[id].zVelocity);
		}
		// gravity
		player.zVelocity -= GRAVITY;
		if (player.zVelocity < TERMINAL_VELOCITY)
			player.zVelocity = TERMINAL_VELOCITY;


		updateClientPositionWithCollision(id, dx, dy, player.zVelocity);
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
	/*
	for (int c = 0; c < 4; c++) {
		// By default, assumes the player is not on the ground
		state->players[c].isGrounded = false;

		// Processes z velocity (jumping & falling)
		updateClientPositionWithCollision(c, 0, 0, state->players[c].zVelocity);

		// Decreases player z velocity by gravity, up to terminal velocity
		state->players[c].zVelocity -= GRAVITY;
		if (state->players[c].zVelocity < TERMINAL_VELOCITY) state->players[c].zVelocity = TERMINAL_VELOCITY;
	}
	*/
}

void ServerGame::applyAttacks()
{
	for (auto& [attackerId, atk] : latestAttacks)
	{
		for (unsigned victimId = 0; victimId < 4; ++victimId)
		{
			if (victimId == attackerId) continue;	// skip self
			if (state->players[victimId].isDead) continue;	// skip dead players
			if (invulTicks[victimId] > 0) continue;	// skip invulnerable players

			if (isHit(atk, state->players[victimId]))
			{
				printf("[HIT] attacker %u hits victim %u (tick %llu)\n",
					attackerId, victimId, state->tick);

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
	if (latestAttacks.empty()) return; // no attacks this tick
	// check if everyone is dead
	bool allDead = true;
	for (unsigned int id = 0; id < num_players; ++id) {
		if (!state->players[id].isDead && !state->players[id].isHunter) {
			allDead = false;
			break;
		}
	}
	if (allDead) {
		printf("[GAME PHASE] All players are dead, cancelling the timer\n");
		timer->cancelTimer(); // cancel the timer
	}
	latestAttacks.clear();
}

void ServerGame::applyDodge()
{
	for (int i = 1; i < 4; i++) {
		int8_t prevDashTick = dashTicks[i];
		if (invulTicks[i] > 0) invulTicks[i]--;
		if (dashTicks[i] > 0) dashTicks[i]--;
		if (dashTicks[i] == 0 && prevDashTick > 0)
		{
			// reset speed
			state->players[i].speed /= DASH_SPEED_MULTIPLIER;

			// players are slowed until end of cooldown
			state->players[i].speed *= DASH_COOLDOWN_PENALTY;
		}
		else if (dashTicks[i] == 0 && (state->tick - lastDodgeTick[i]) >= dodgeCooldownTicks[i]) {
			// end of cooldown
			dashTicks[i] = -1; // prevents from happening multiple times
			state->players[i].speed /= DASH_COOLDOWN_PENALTY;
		}
	}
}

// -----------------------------------------------------------------------------
// NETWORK
// -----------------------------------------------------------------------------

void ServerGame::sendGameStateUpdates() {

	char packet_data[HDR_SIZE + sizeof(GameState)];

	NetworkServices::buildPacket<GameState>(PacketType::GAME_STATE, *state, packet_data);

	network->sendToAll(packet_data, HDR_SIZE + sizeof(GameState));
}

void ServerGame::sendAppPhaseUpdates() {

	char packet_data[HDR_SIZE + sizeof(AppPhasePayload)];

	AppPhasePayload* data = new AppPhasePayload{
		.phase = appState->gamePhase
	};

	printf("GAME PHASE = %d\n", appState->gamePhase);

	NetworkServices::buildPacket<AppPhasePayload>(PacketType::APP_PHASE, *data, packet_data);

	network->sendToAll(packet_data, HDR_SIZE + sizeof(AppPhasePayload));
}

void ServerGame::sendShopOptions(ShopOptionsPayload* data, int dest) {
	char packet_data[HDR_SIZE + sizeof(ShopOptionsPayload)];

	NetworkServices::buildPacket<ShopOptionsPayload>(PacketType::SHOP_INIT, *data, packet_data);

	network->sendToClient(dest, packet_data, HDR_SIZE + sizeof(ShopOptionsPayload));
}

// -----------------------------------------------------------------------------
// PHYSICS
// -----------------------------------------------------------------------------

void ServerGame::updateClientPositionWithCollision(unsigned int clientId, float dx, float dy, float dz) {
	// Update the position with collision detection
	float delta[3] = { dx, dy, dz };

	// Bounding box for the current client
	float playerRadius = 1.0f * PLAYER_SCALING_FACTOR;

	// Bounding box before player moves
	BoundingBox staticPlayerBox = {
			state->players[clientId].x - playerRadius,
			state->players[clientId].y - playerRadius,
			state->players[clientId].z - playerRadius,
			state->players[clientId].x + playerRadius,
			state->players[clientId].y + playerRadius,
			state->players[clientId].z + playerRadius
	};

	for (int i = 0; i < 3; i++) {
		bool isColliding = false;

		BoundingBox playerBox = {
			state->players[clientId].x - playerRadius,
			state->players[clientId].y - playerRadius,
			state->players[clientId].z - playerRadius,
			state->players[clientId].x + playerRadius,
			state->players[clientId].y + playerRadius,
			state->players[clientId].z + playerRadius
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
		for (int c = 0; c < num_players; c++) {
			// skip current client
			if (c == (int)clientId) {
				continue;
			}

			BoundingBox otherClientBox = {
				state->players[c].x - playerRadius,
				state->players[c].y - playerRadius,
				state->players[c].z - playerRadius,
				state->players[c].x + playerRadius,
				state->players[c].y + playerRadius,
				state->players[c].z + playerRadius,
			};

			if (checkCollision(playerBox, otherClientBox)) {
				// printf("[CLIENT %d] Collision detected with client %d, axis=%d\n", clientId, c, i);

				float distance = findDistance(staticPlayerBox, otherClientBox, i) * (delta[i] > 0 ? 1 : -1);
				if (abs(distance) < abs(delta[i])) delta[i] = distance;

				// If the z is being changed, reset z velocity and "ground" player
				if (i == 2) {
					// Check with zVelocity, not dz
					if (state->players[clientId].zVelocity < 0) state->players[clientId].isGrounded = true;
					state->players[clientId].zVelocity = 0;
					// printf("[CLIENT %d] Collision with player detected. isGrounded=%d, zVelocity=%f\n", clientId, state->players[clientId].isGrounded ? 1 : 0, state->players[clientId].zVelocity);
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
				// If the z is being changed DOWNWARDS and collides, reset z velocity and "ground" player
				if (i == 2 && playerBox.minZ < boxes2d[b].maxZ && delta[2] < 0) {
					// printf("[CLIENT %d] Collision DOWNWARD with box detected. zVelocity=%f, deltaZ=%f\n", clientId,  state->players[clientId].zVelocity, delta[2]);

					// newZ - playerRadius	= surfaceZ
					// newZ					= surfaceZ + playerRadius
					// currentZ + deltaZ	= surfaceZ + playerRadius
					// deltaZ				= surfaceZ + playerRadius - currentZ
					delta[2] = boxes2d[b].maxZ + playerRadius - state->players[clientId].z;
					state->players[clientId].isGrounded = true;
					state->players[clientId].zVelocity = 0;
					/*
					printf("BOXID=%llu, box.minZ=%f, box.maxZ=%f\n", b, boxes2d[b].minZ, boxes2d[b].maxZ);
					printf("Dynamic playerbox: PLAYERBOX.minZ=%f, PLAYERBOX.maxZ=%f\n", playerBox.minZ, playerBox.maxZ);
					printf("Static  playerbox: PLAYERBOX.minZ=%f, PLAYERBOX.maxZ=%f\n", staticPlayerBox.minZ, staticPlayerBox.maxZ);
					*/
				}
				else {
					float distance = findDistance(staticPlayerBox, boxes2d[b], i) * (delta[i] > 0 ? 1 : -1);
					if (abs(distance) < abs(delta[i])) delta[i] = distance;
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
		if (delta[2] < 0) state->players[clientId].isGrounded = true;
		// printf("[CLIENT %d] Collision with z-plane detected. isGrounded=%d, zVelocity=%f\n", clientId, state->players[clientId].isGrounded ? 1 : 0, state->players[clientId].zVelocity);
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

// -----------------------------------------------------------------------------
// BOUNDING BOXES
// -----------------------------------------------------------------------------

// Finds the distance between two bounding boxes on one axis
static float findDistance(BoundingBox box1, BoundingBox box2, char direction) {
	if (direction == 0) // X axis
		return min(abs(box1.minX - box2.maxX), abs(box1.maxX - box2.minX));
	if (direction == 1) // Y axis
		return min(abs(box1.minY - box2.maxY), abs(box1.maxY - box2.minY));
	if (direction == 2) // Z axis
		return min(abs(box1.minZ - box2.maxZ), abs(box1.maxZ - box2.minZ));
}

void ServerGame::readBoundingBoxes() {
	static std::random_device rd;                                    // seed source
	static std::mt19937       gen(rd());                             // mersenne twister engine
	static std::uniform_int_distribution<int> distRGBA(150, 255);    // for R,G,B
	const wchar_t* fileAddr = L"bb#_bboxes.json";
	
	
	Slice<BYTE> fileData;
	DX::ReadDataStatus readStatus = DX::ReadDataToSlice(fileAddr, fileData, true);
	if (readStatus != DX::ReadDataStatus::SUCCESS) {
		fwprintf(stderr, L"Cannot read file %s\n", fileAddr);
	};

	JSON_Value* rootVal = json_parse_string(reinterpret_cast<char*>(fileData.ptr));
	free(fileData.ptr);

	if (!rootVal) { fwprintf(stderr, L"Cannot parse %s\n", fileAddr); }
	else {wprintf(L"Parsed %s\n", fileAddr);}
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