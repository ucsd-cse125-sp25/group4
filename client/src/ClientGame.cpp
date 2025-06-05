#include "ClientGame.h"
#include <algorithm>
#include <string>
#include <iostream>
using namespace std;
const wchar_t CLASS_NAME[] = L"Window Class";
const wchar_t GAME_NAME[] = L"$GAME_NAME";


ClientGame::ClientGame(HINSTANCE hInstance, int nCmdShow, string IPAddress) {
	network = new ClientNetwork(IPAddress);

	InitPayload init{};  // empty payload for now
	char packet_data[HDR_SIZE + sizeof(InitPayload)];

	NetworkServices::buildPacket<InitPayload>(PacketType::INIT_CONNECTION, init, packet_data);

	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(InitPayload));

	WNDCLASSEX windowClass = { 
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WindowProc,
		.hInstance = hInstance,
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.lpszClassName = L"DXSampleClass",
	};
    RegisterClassEx(&windowClass);
	
	// TODO: pass this into the renderer intialization
    RECT windowRect = { 0, 0, 1920/2, 1080/2};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);


	// initialize the state
	HWND windowHandle = CreateWindow(
        windowClass.lpszClassName,
        GAME_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        this);

	
	if (!windowHandle) {
		printf("Failed to create window\n");
		exit(1);
	}

	if (!renderer.Init(windowHandle)) {
		OutputDebugString(L"Failed to initalize renderer\n");
		exit(1);
	}

	if (windowHandle == NULL) {
		exit(1);
	}

	ShowWindow(windowHandle, nCmdShow);
	ShowCursor(FALSE);
	hwnd = windowHandle;

	appState = new AppState();
	appState->gamePhase = GamePhase::START_MENU;
	appState->gameState = gameState;

	audioEngine->Init();

	audioEngine->LoadSound(a_music, true, true);
	audioEngine->LoadSound(a_jump, true, false);
	audioEngine->LoadSound(a_attack, true, false);
	audioEngine->LoadSound(a_bear, true, false);
	audioEngine->LoadSound(a_dodge, true, false);
	audioEngine->LoadSound(a_move_1, true, false);
	audioEngine->LoadSound(a_move_2, true, false);
	audioEngine->LoadSound(a_move_3, true, false);
	audioEngine->LoadSound(a_move_4, true, false);
	audioEngine->LoadSound(a_purchase, true, false);
	audioEngine->LoadSound(a_round_end, true, false);
	audioEngine->LoadSound(a_round_start, true, false);
	audioEngine->LoadSound(a_darkness, true, false);

	bgmChannel = audioEngine->PlayOneSound(a_music, { 0, 0, 0 }, 0.5f);

	uint8_t initPowerups[4][20];
	
	// debugging, ignore
	//memset(initPowerups, 1, 20);
	//memset((&initPowerups[0][0] + 20), 2, 20);
	//memset((&initPowerups[0][0] + 40), 3, 20);
	//memset((&initPowerups[0][0] + 60), 103, 20);
	memset(initPowerups, 255, sizeof(initPowerups));
	renderer.updatePlayerPowerups(&initPowerups[0][0]);

	jumpWasDown = false;
	dodgeWasDown = false;
	attackWasDown = false;

	localAnimState.curAnims[0] = HunterAnimation::HUNTER_ANIMATION_IDLE;
	localAnimState.isLoop[0] = true;
	// TODO runner IDLE anim!
	for (int i = 1; i < 4; i++) {
		localAnimState.curAnims[i] = RunnerAnimation::RUNNER_ANIMATION_WALK;
		localAnimState.curAnims[i] = true;
	}

	renderer.players[0].loopAnimation(HunterAnimation::HUNTER_ANIMATION_IDLE);
	renderer.players[1].loopAnimation(RunnerAnimation::RUNNER_ANIMATION_WALK);
	renderer.players[2].loopAnimation(RunnerAnimation::RUNNER_ANIMATION_WALK);
	renderer.players[3].loopAnimation(RunnerAnimation::RUNNER_ANIMATION_WALK);
}

void ClientGame::sendDebugPacket(const char* message) {
	DebugPayload dbg{};
	sprintf_s(dbg.message, sizeof(dbg.message), message);

	char packet_data[HDR_SIZE + sizeof(DebugPayload)];
	NetworkServices::buildPacket<DebugPayload>(PacketType::DEBUG, dbg, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(DebugPayload));
}

void ClientGame::sendMovePacket(float direction[3], float yaw, float pitch, bool jump) {
	MovePayload mv{};
	for (int i = 0; i < 3; i++)
		mv.direction[i] = direction[i];
	mv.yaw = yaw;
	mv.pitch = pitch;
	mv.jump = jump;

	char packet_data [HDR_SIZE + sizeof(MovePayload)];
	NetworkServices::buildPacket<MovePayload>(PacketType::MOVE, mv, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(MovePayload));
}

void ClientGame::sendCameraPacket(float yaw, float pitch) {
	CameraPayload cam{};
	cam.yaw = yaw;
	cam.pitch = pitch;

	char buf[HDR_SIZE + sizeof(cam)];
	NetworkServices::buildPacket(PacketType::CAMERA, cam, buf);
	NetworkServices::sendMessage(network->ConnectSocket, buf, sizeof buf);
}

void ClientGame::sendReadyStatusPacket(uint8_t selection = 0) {
	PlayerReadyPayload status{};
	status.ready = true;
	if (appState->gamePhase == GamePhase::SHOP_PHASE)
	{
		status.selection = selection;
	}

	char buf[HDR_SIZE + sizeof(status)];
	NetworkServices::buildPacket(PacketType::PLAYER_READY, status, buf);
	NetworkServices::sendMessage(network->ConnectSocket, buf, sizeof buf);
}

void ClientGame::sendAttackPacket(float origin[3], float yaw, float pitch) {
	AttackPayload atk{};
	atk.originX = origin[0];
	atk.originY = origin[1];
	atk.originZ = origin[2];
	atk.yaw = yaw;
	atk.pitch = pitch;
	atk.range = ATTACK_RANGE;

	char packet_data[HDR_SIZE + sizeof(AttackPayload)];
	NetworkServices::buildPacket(PacketType::ATTACK, atk, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket,
		packet_data,
		sizeof packet_data);
}

void ClientGame::sendDodgePacket()
{
	DodgePayload dp{ yaw, pitch };
	char buf[HDR_SIZE + sizeof dp];
	NetworkServices::buildPacket(PacketType::DODGE, dp, buf);
	NetworkServices::sendMessage(network->ConnectSocket, buf, sizeof buf);
}

void ClientGame::sendBearPacket()
{
	BearPayload bp{ };
	char buf[HDR_SIZE + sizeof bp];
	NetworkServices::buildPacket(PacketType::BEAR, bp, buf);
	NetworkServices::sendMessage(network->ConnectSocket, buf, sizeof buf);
}

void ClientGame::sendPhantomPacket()
{
	PhantomPayload pp{ };
	char buf[HDR_SIZE + sizeof pp];
	NetworkServices::buildPacket(PacketType::PHANTOM, pp, buf);
	NetworkServices::sendMessage(network->ConnectSocket, buf, sizeof buf);
}

void ClientGame::update() {

	// check for server updates and process them accordingly
	int len = network->receivePackets(network_data);
	while (len > 0) {
		// here, network_data should contain the game state packet
		PacketHeader* hdr = (PacketHeader*)network_data;
		switch (hdr->type) {
		case PacketType::GAME_STATE: 
		{
			// printf("received update for tick %llu \n", game_state->tick);
			gameState = (GameState*)(network_data + HDR_SIZE);
			//char msgbuf[1000];
			// printf(msgbuf, "Packet received y=%f \n", state->position[1]);

			for (int i = 0; i < 4; i++) {
				renderer.players[i].pos.x = gameState->players[i].x;
				renderer.players[i].pos.y = gameState->players[i].y;
				renderer.players[i].pos.z = gameState->players[i].z;
				renderer.players[i].isHunter = gameState->players[i].isHunter;  // NEW

				// update the rotation from other players only (only if not spectator, otherwise gotta update everything) (only for game phase)
				if (id != 4 && i == renderer.currPlayer.playerId && appState->gamePhase == GamePhase::GAME_PHASE) continue;
				renderer.players[i].lookDir.pitch = gameState->players[i].pitch;
				renderer.players[i].lookDir.yaw = gameState->players[i].yaw;
			}

			// cache own dead flag for input handling
			localDead = gameState->players[renderer.currPlayer.playerId].isDead;

			// update timer
			renderer.updateTimer(gameState->timerFrac);

			playAudio();
			
			break;
		}
		case PacketType::DEBUG: 
		{
			break;
		}
		case PacketType::IDENTIFICATION:
		{
			IDPayload* idPayload = (IDPayload*)(network_data + HDR_SIZE);

			id = idPayload->id;
			if (id != 4) {
				renderer.currPlayer.playerId = id;
			}
			else {
				renderer.currPlayer.playerId = 0;
			}

			char message[128];

			strcpy_s(message, std::to_string(id).c_str());
			strcat_s(message, " is my ID");

			sendDebugPacket(message);

			break;
		}
		case PacketType::ACTION_OK:
		{
			ActionOkPayload* ok = reinterpret_cast<ActionOkPayload*>(network_data + HDR_SIZE);
			PacketType action = (PacketType)ok->packetType;
			switch (action) {
			case PacketType::DODGE:
				if (ok->id == renderer.currPlayer.playerId) {
					audioEngine->PlayOneSound(a_dodge, { 0,0,0 }, 1);
				}
				break;
			case PacketType::ATTACK:
				audioEngine->PlayOneSound(a_attack, { 0,0,0 }, 1);
				break;
			case PacketType::BEAR:
				audioEngine->PlayOneSound(a_bear, { 0,0,0 }, 1);
				break;
			case PacketType::MOVE:
				if (ok->id == renderer.currPlayer.playerId) {
					audioEngine->PlayOneSound(a_jump, { 0,0,0 }, 1);
				}
				break;
			case PacketType::SHOP_UPDATE:
				audioEngine->PlayOneSound(a_purchase, { 0,0,0 }, 1);
				break;
			default:
				break;
			}
			//if (action == PacketType::NOCTURNAL) {
			//	audioEngine->PlayOneSound(a_darkness, { 0,0,0 }, 1);
			//}
			
			// Optional: kick off a local dash animation / speed buff here.
			printf(">> %d granted!\n", action);
			break;
		}

		case PacketType::APP_PHASE:
		{
			AppPhasePayload* statusPayload = (AppPhasePayload*)(network_data + HDR_SIZE);

			appState->gamePhase = statusPayload->phase;
			renderer.gamePhase = statusPayload->phase;

			if (statusPayload->phase == GamePhase::GAME_PHASE) {
				audioEngine->PlayOneSound(a_round_start, { 0,0,0 }, 1);
			}
			else if (statusPayload->phase == GamePhase::GAME_END) {
				audioEngine->PlayOneSound(a_round_end, { 0,0,0 }, 1);
				audioEngine->StopChannel(bgmChannel);
			}
			else if (statusPayload->phase == GamePhase::START_MENU) {
				bgmChannel = audioEngine->PlayOneSound(a_music, { 0,0,0 }, 0.5f);
			}

			ready = false;
			
			break;
		}
		case PacketType::SHOP_INIT:
		{
			ShopOptionsPayload* optionsPayload = (ShopOptionsPayload*)(network_data + HDR_SIZE);

			ready = false;
			
			appState->gamePhase = GamePhase::SHOP_PHASE;
			renderer.gamePhase = GamePhase::SHOP_PHASE;

			for (int i = 0; i < NUM_POWERUP_OPTIONS; i++)
			{
				Powerup powerup = (Powerup)optionsPayload->options[i];
				shopOptions[i].item = powerup;
				shopOptions[i].isSelected = false;
				shopOptions[i].isBuyable = (PowerupInfo[powerup].cost <= gameState->players[id].coins);
			}
			renderer.updatePowerups(shopOptions[0].item, shopOptions[1].item, shopOptions[2].item);

			if (id == 0) {
				// really sketch isHunter check...
				renderer.updateCurrency(gameState->players[id].coins, optionsPayload->hunter_score);
			}
			else {
				renderer.updateCurrency(gameState->players[id].coins, optionsPayload->runner_score);
			}

			break;
		}
		case PacketType::PLAYER_POWERUPS:
		{
			PlayerPowerupPayload* pwPayload = (PlayerPowerupPayload*)(network_data + HDR_SIZE);
			
			bunnyhop = false;

			for (int i = 0; i < 20; i++)
			{
				powerups[i] = pwPayload->powerupInfo[id][i];
				if (pwPayload->powerupInfo[id][i] == (uint8_t)Powerup::H_BUNNY_HOP ||
					pwPayload->powerupInfo[id][i] == (uint8_t)Powerup::R_BUNNY_HOP) {
					bunnyhop = true;
				}
			}
			renderer.updatePlayerPowerups(&pwPayload->powerupInfo[0][0]);
			break;
		}
		case PacketType::ANIMATION_STATE:
		{
			AnimationState* remoteAnimState = (AnimationState*)(network_data + HDR_SIZE);
			for (int i = 0; i < 4; i++) {
				if (remoteAnimState->curAnims[i] != localAnimState.curAnims[i]) {
					localAnimState.curAnims[i] = remoteAnimState->curAnims[i];
					localAnimState.isLoop[i] = remoteAnimState->isLoop[i];
					if (i == 0) {
						if (remoteAnimState->isLoop) {
							renderer.players[i].loopAnimation((HunterAnimation)remoteAnimState->curAnims[i]);
						}
						else {
							renderer.players[i].playAnimationToEnd((HunterAnimation)remoteAnimState->curAnims[i]);
						}
					}
					else {
						if (remoteAnimState->isLoop) {
							renderer.players[i].loopAnimation((RunnerAnimation)remoteAnimState->curAnims[i]);
						}
						else {
							renderer.players[i].playAnimationToEnd((RunnerAnimation)remoteAnimState->curAnims[i]);
						}
					}
				}
			}
			break;
		}
		default:
			// printf("error in packet type %d, expected GAME_STATE or DEBUG\n", hdr->type);
			break;
		}
		len = network->receivePackets(network_data);
	}

	// ---------------------------------------------------------------	
	// Client Input Handling 

	if (id != -1 && id != 4) {
		handleInput();
	}
	else if (id == 4) {
		handleSpectatorInput();
	}

	// ---------------------------------------------------------------	
	// Update GPU data and render 
	renderer.updateCamera(yaw, pitch);
	// copy new data to the GPU
	renderer.OnUpdate();
	// render the frame
	// this will block if 2 frames have been sent to the GPU and none have been drawn 
	bool success = renderer.Render(); // render function

}

ClientGame::~ClientGame() {
	delete network;
	delete audioEngine;
}

// ──────────────────────────────────────────────────────────────────
// Refactor handleInput()
inline bool ClientGame::isWindowFocused() const
{
	return GetForegroundWindow() == hwnd;
}

bool ClientGame::processCameraInput()
{
	POINT  p;  GetCursorPos(&p);
	RECT   rc; GetClientRect(hwnd, &rc);
	POINT centre{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
	ClientToScreen(hwnd, &centre);

	int dx = p.x - centre.x;
	int dy = p.y - centre.y;
	if (!dx && !dy) return false;

	yaw += -dx * MOUSE_SENS;
	pitch += -dy * MOUSE_SENS; // invert y makes more sense
	pitch = std::clamp(pitch,
		XMConvertToRadians(-89.0f),
		XMConvertToRadians(+89.0f));

	SetCursorPos(centre.x, centre.y);
	sendCameraPacket(yaw, pitch);

	// update own camera locally for immediate feedback
	auto& meLook = renderer.players[renderer.currPlayer.playerId].lookDir;
	meLook.pitch = pitch;
	meLook.yaw = yaw;
	return true;
}

bool ClientGame::processSpectatorCameraInput()
{
	POINT  p;  GetCursorPos(&p);
	RECT   rc; GetClientRect(hwnd, &rc);
	POINT centre{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
	ClientToScreen(hwnd, &centre);

	int dx = p.x - centre.x;
	int dy = p.y - centre.y;
	if (!dx && !dy) return false;

	yaw += -dx * MOUSE_SENS;
	pitch += -dy * MOUSE_SENS; // invert y makes more sense
	pitch = std::clamp(pitch,
		XMConvertToRadians(-89.0f),
		XMConvertToRadians(+89.0f));

	SetCursorPos(centre.x, centre.y);
	// spectator does not update player model orientation, server updates does.
	return true;
}

void ClientGame::processSpectatorKeyboardInput()
{
	bool detachKeyDown = (GetAsyncKeyState('5') & 0x8000) != 0;
	if (detachKeyDown && !renderer.detached) {
		using namespace DirectX;
		XMVECTOR playerPos = XMLoadFloat3(&renderer.players[renderer.currPlayer.playerId].pos);
		XMVECTOR model_fwd = XMVectorSet(0, 1, 0, 0);
		XMVECTOR rotation = XMVector3TransformNormal(model_fwd, XMMatrixRotationX(pitch) * XMMatrixRotationZ(yaw));
		rotation = XMVector3Normalize(rotation);
		// compute camPos exaclty like computeViewProject
		static constexpr float FREECAM_DIST = Renderer::CAMERA_DIST;
		static constexpr float FREECAM_UP = Renderer::CAMERA_UP;
		XMVECTOR camPos = XMVectorSubtract(playerPos, XMVectorScale(rotation, FREECAM_DIST));
		camPos = XMVectorAdd(camPos, XMVectorSet(0, 0, FREECAM_UP, 0));

		XMStoreFloat3(&renderer.freecamPos, camPos);
		renderer.detached = true;
	}

	if (GetAsyncKeyState('1') & 0x8000) {
		renderer.currPlayer.playerId = 0;
		renderer.detached = false;
	}
	if (GetAsyncKeyState('2') & 0x8000) {
		renderer.currPlayer.playerId = 1;
		renderer.detached = false;
	}
	if (GetAsyncKeyState('3') & 0x8000) {
		renderer.currPlayer.playerId = 2;
		renderer.detached = false;
	}
	if (GetAsyncKeyState('4') & 0x8000) {
		renderer.currPlayer.playerId = 3;
		renderer.detached = false;
	}

	if (renderer.detached) {
		using namespace DirectX;
		XMVECTOR model_fwd = XMVectorSet(0, 1, 0, 0);
		XMVECTOR forward = XMVector3TransformNormal(model_fwd, XMMatrixRotationX(pitch) * XMMatrixRotationZ(yaw));
		forward = XMVector3Normalize(forward);

		XMVECTOR model_up = XMVectorSet(0, 0, 1, 0);
		XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, model_up));

		float moveSpeed = 0.025;

		if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
			moveSpeed /= 2;
		}

		XMVECTOR pos = XMLoadFloat3(&renderer.freecamPos);

		if (GetAsyncKeyState('W') & 0x8000) {
			pos = XMVectorAdd(pos, XMVectorScale(forward, moveSpeed));
		}
		if (GetAsyncKeyState('S') & 0x8000) {
			pos = XMVectorSubtract(pos, XMVectorScale(forward, moveSpeed));
		}
		if (GetAsyncKeyState('A') & 0x8000) {
			pos = XMVectorSubtract(pos, XMVectorScale(right, moveSpeed));
		}
		if (GetAsyncKeyState('D') & 0x8000) {
			pos = XMVectorAdd(pos, XMVectorScale(right, moveSpeed));
		}

		XMStoreFloat3(&renderer.freecamPos, pos);
	}
}

bool ClientGame::processMovementInput()
{
	float direction[3] = { 0, 0, 0 };
	bool jump = false;
	if (GetAsyncKeyState('W') & 0x8000) direction[0] += 1;
	if (GetAsyncKeyState('S') & 0x8000) direction[0] -= 1;
	if (GetAsyncKeyState('A') & 0x8000) direction[1] -= 1;
	if (GetAsyncKeyState('D') & 0x8000) direction[1] += 1;

	// if hunter phantom, hold space to fly up and hold control to fly down
	if (renderer.currPlayer.playerId == 0 && gameState->players[id].isPhantom) 
	{
		if (GetAsyncKeyState(' ') & 0x8000) direction[2] += 1; // up
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000) direction[2] -= 1; // down
	}
	else 
	{
		bool jumpNowDown = (GetAsyncKeyState(' ') & 0x8000) != 0;

	if (jumpNowDown && (!jumpWasDown || bunnyhop)) {     // rising edge
		jump = true;
	}

	jumpWasDown = jumpNowDown;
	}

	if (!direction[0] && !direction[1] && !direction[2] && !jump) return false;
	sendMovePacket(direction, yaw, pitch, jump);
	return true;
}

// 5) Hunter’s left‑click attack, only for client‑0
void ClientGame::processAttackInput()
{
	if (renderer.currPlayer.playerId != 0) return;   // only hunter
	bool attackNowDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	if (attackNowDown && !attackWasDown)            // rising edge
	{
		float pos[3] = {
			renderer.players[0].pos.x,
			renderer.players[0].pos.y,
			renderer.players[0].pos.z
		};
		sendAttackPacket(pos, yaw, pitch);
		renderer.players[id].playAnimationToEnd(HUNTER_ANIMATION_ATTACK); //TODO: MOVE TO ACTION_OK PACKET HANDLING???
	}
	attackWasDown = attackNowDown;
}

void ClientGame::processDodgeInput()
{
	if (renderer.currPlayer.playerId == 0) return;   // hunter cannot dash
	if (gameState->players[id].isBear) return;		 // bear cannot dash

	bool dodgeNowDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

	if (dodgeNowDown && !dodgeWasDown)      // rising edge
		sendDodgePacket();

	dodgeWasDown = dodgeNowDown;
}

void ClientGame::processBearInput()
{
	if (renderer.currPlayer.playerId == 0) return;   // hunter cannot bear
	if (gameState->players[id].isBear) return;		 // bear cannot bear

	static bool rWasDown = false;
	bool rNowDown = (GetAsyncKeyState('E') & 0x8000) != 0;

	if (rNowDown && !rWasDown)      // rising edge
		sendBearPacket();

	rWasDown = rNowDown;
}

void ClientGame::processPhantomInput()
{
	if (renderer.currPlayer.playerId != 0) return;   // runner cannot phantom
	if (gameState->players[id].isPhantom) return;	 // phantom cannot phantom
	static bool rWasDown = false;
	bool rNowDown = (GetAsyncKeyState('E') & 0x8000) != 0;
	if (rNowDown && !rWasDown)      // rising edge
		sendPhantomPacket();
	rWasDown = rNowDown;
}

void ClientGame::processShopInputs() {
	// if ready, player is locked in and cannot change
	if (ready)
		return;

	static bool wasDown1 = false;
	static bool wasDown2 = false;
	static bool wasDown3 = false;
	bool down1 = (GetAsyncKeyState('1') & 0x8000) != 0;
	bool down2 = (GetAsyncKeyState('2') & 0x8000) != 0;
	bool down3 = (GetAsyncKeyState('3') & 0x8000) != 0;

	// only allow one selection per tick
	if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
		ready = true;
		gameState->players[id].coins = tempCoins;
		uint8_t selection = 0;

		// only one powerup can be selected
		for (auto& item : shopOptions) {
			if (item.isSelected) {
				selection = (uint8_t)item.item;
			}
		}
		// TODO: display powerup in GUI?
		// things to consider:
		// powerups can be passive (trap, buff) or active (item, temporary)
		// can't have multiple of the same powerup
		// 1 purchase per shop
		// how should it be displayed/ordered?
		
		//storePowerups(selection);
		sendReadyStatusPacket(selection);
	}
	else if (!down1 && wasDown1) {
		handleShopItemSelection(0);
	}
	else if (!down2 && wasDown2) {
		handleShopItemSelection(1);
	}
	else if (!down3 && wasDown3) {
		handleShopItemSelection(2);
	}
	wasDown1 = down1;
	wasDown2 = down2;
	wasDown3 = down3;
}
 
void ClientGame::handleShopItemSelection(int choice) {
	ShopItem* item = &(shopOptions[choice]);
	int cost = PowerupInfo[item->item].cost;
	if (item->isSelected) 
	{
		item->isSelected = false;
		renderer.selectPowerup(3); // exceeds option
		tempCoins += cost;
	}
	else
	{
		// Only select the item if client has enough coins
		if (item->isBuyable)
		{
			for (int i = 0; i < NUM_POWERUP_OPTIONS; i++)
			{
				shopOptions[i].isSelected = (i == choice);
			}
			renderer.selectPowerup((uint8_t) choice);
			tempCoins -= cost;
		}
	}
}

void ClientGame::playAudio()
{
	static bool wasBear = false;
	static bool wasPhantom = false;
	bool isBear = false;
	bool isPhantom = false;

	for (int i = 0; i < 4; i++) {
		if (gameState->players[i].isBear)
			isBear = true;
		if (gameState->players[i].isPhantom)
			isPhantom = true;
	}

	if (!wasBear && isBear)
	{
		audioEngine->PlayOneSound(a_bear, { 0,0,0 }, 1);
	}
	if (!wasPhantom && isPhantom)
	{
		// play phantom audio
	}
	wasBear = isBear;
	wasPhantom = isPhantom;
}

void ClientGame::handleInput()
{
	if (!isWindowFocused()) return;

	bool tabDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	if (tabDown && !renderer.activeScoreboard) {
		renderer.activeScoreboard = true;
	}
	else if (!tabDown && renderer.activeScoreboard) {
		renderer.activeScoreboard = false;
	}

	switch (appState->gamePhase)
	{
	case GamePhase::START_MENU:
	case GamePhase::GAME_END:
	{
		yaw = startYaw;
		pitch = startPitch;
		
		// Avoid sending multiple ready packets
		if (ready)
			break;

		if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
			ready = true;
			sendReadyStatusPacket();
		}

		break;
	}
	case GamePhase::SHOP_PHASE:
	{
		processShopInputs();
		break;
	}
	case GamePhase::GAME_PHASE:
	{

		// camera is always allowed (even dead players can spectate)
		processCameraInput();

		// if you’re dead, no movement or attack
		if (localDead) return;

		// movement & attack for the living
		processMovementInput();
		processAttackInput();
		processDodgeInput();
		processBearInput();
		processPhantomInput();
		break;
	}
	default:
	{
		break;
	}
	}
}

void ClientGame::handleSpectatorInput()
{
	if (!isWindowFocused()) return;

	bool tabDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	if (tabDown && !renderer.activeScoreboard) {
		renderer.activeScoreboard = true;
	}
	else if (!tabDown && renderer.activeScoreboard) {
		renderer.activeScoreboard = false;
	}

	switch (appState->gamePhase)
	{
	case GamePhase::START_MENU:
	case GamePhase::GAME_END:
	{
		// TODO: spectator logic should be same (focus on player) for all game phase
		// except shop, shop UI should be special for spectator
		yaw = startYaw;
		pitch = startPitch;
		break;
	}
	case GamePhase::SHOP_PHASE:
	{
		//processShopInputs();
		break;
	}
	case GamePhase::GAME_PHASE:
	{

		// camera is always allowed (even dead players can spectate)
		processSpectatorKeyboardInput();
		processSpectatorCameraInput();
		break;
	}
	default:
	{
		break;
	}
	}
}

inline ClientGame *GetState(HWND window_handle) {
	LONG_PTR ptr = GetWindowLongPtr(window_handle, GWLP_USERDATA);
	ClientGame *state = reinterpret_cast<ClientGame *>(ptr);
	return state;
}

LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	ClientGame *state;
	if (uMsg == WM_CREATE) {
		CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
		state = reinterpret_cast<ClientGame *>(pCreate->lpCreateParams);
		SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR)state);
	}
	else {
		state = GetState(window_handle);
	}
	switch (uMsg) { 
	case WM_SIZE:
	{
		int width = LOWORD(lParam); // get low order word

		int height = HIWORD(lParam); // get high order word
		
		// respond to the message
		// TODO: implement
		// OnSize(window_handle, (UINT)wParam, width, height);
	}
	break;
	case WM_PAINT:
	{
		// this is NOT called every frame
		state->renderer.OnUpdate();
		bool success = state->renderer.Render(); // render function

	}
	break;
	case WM_CLOSE:
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;
	case WM_KEYDOWN:
	{
		/*
		if (wParam == VK_DOWN) {
			state->renderer.dbg_NumTrisToDraw -= 3;
		}
		if (wParam == VK_UP) {
			state->renderer.dbg_NumTrisToDraw += 3;
		}
		break;
		*/	
	}
	} 

	return DefWindowProc(window_handle, uMsg, wParam, lParam);
} 