#include "ClientGame.h"
#include <algorithm>

const wchar_t CLASS_NAME[] = L"Window Class";
const wchar_t GAME_NAME[] = L"$GAME_NAME";

ClientGame::ClientGame(HINSTANCE hInstance, int nCmdShow) {
	network = new ClientNetwork();

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
}

void ClientGame::sendDebugPacket(const char* message) {
	DebugPayload dbg{};
	sprintf_s(dbg.message, sizeof(dbg.message), message);

	char packet_data[HDR_SIZE + sizeof(DebugPayload)];
	NetworkServices::buildPacket<DebugPayload>(PacketType::DEBUG, dbg, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(DebugPayload));
}

/*
void ClientGame::sendGameStatePacket(float posDelta[4]) {
	GameState* state = new GameState{ {0} };
	//sprintf_s(state.position, sizeof(state.position), "debug: tick %llu", tick);
	for (int i = 0; i < 4; i++) {
		state->position[id][i] = posDelta[i];
	}

	char packet_data[HDR_SIZE + sizeof(GameState)];
	NetworkServices::buildPacket<GameState>(PacketType::MOVE, *state, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(GameState));
	delete state;
}
*/

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

void ClientGame::update() {

	// check for server updates and process them accordingly
	int len = network->receivePackets(network_data);
	if (len > 0) {
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
				//renderer.players[i].isDead = state->players[i].isDead;      // NEW
				//renderer.players[i].isHunter = state->players[i].isHunter;  // NEW

				// update the rotation from other players only.
				if (i == renderer.currPlayer.playerId) continue;
				renderer.players[i].lookDir.pitch = gameState->players[i].pitch;
				renderer.players[i].lookDir.yaw = gameState->players[i].yaw;
			}

			// cache own “dead” flag for input handling
			localDead = gameState->players[renderer.currPlayer.playerId].isDead;

			
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
			renderer.currPlayer.playerId = id;
			char message[128];

			strcpy_s(message, std::to_string(id).c_str());
			strcat_s(message, " is my ID");

			sendDebugPacket(message);

			break;
		}
		case PacketType::DODGE_OK:
		{
			DodgeOkPayload* ok = reinterpret_cast<DodgeOkPayload*>(network_data + HDR_SIZE);
			//invulFrames_ = ok->invulTicks;        // usually 30
			// Optional: kick off a local dash animation / speed buff here.
			printf(">> DODGE granted!\n");
			break;
		}

		case PacketType::APP_PHASE:
		{
			AppPhasePayload* statusPayload = (AppPhasePayload*)(network_data + HDR_SIZE);

			appState->gamePhase = statusPayload->phase;
			renderer.gamePhase = statusPayload->phase;

			ready = false;
			
			break;
		}
		case PacketType::SHOP_INIT:
		{
			ShopOptionsPayload* optionsPayload = (ShopOptionsPayload*)(network_data + HDR_SIZE);

			for (int i = 0; i < NUM_POWERUP_OPTIONS; i++)
			{
				Powerup powerup = (Powerup)optionsPayload->options[i];
				shopOptions[i].item = powerup;
				shopOptions[i].isSelected = false;
				shopOptions[i].isBuyable = (PowerupCosts[powerup] <= gameState->players[id].coins);
			}

			break;
		}
		default:
			// printf("error in packet type %d, expected GAME_STATE or DEBUG\n", hdr->type);
			break;
		}

	}

	// ---------------------------------------------------------------	
	// Client Input Handling 

	handleInput();

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

bool ClientGame::processMovementInput()
{
	float direction[3] = { 0, 0, 0 };
	bool jump = false;
	if (GetAsyncKeyState('W') & 0x8000) direction[0] += 1;
	if (GetAsyncKeyState('S') & 0x8000) direction[0] -= 1;
	if (GetAsyncKeyState('A') & 0x8000) direction[1] -= 1;
	if (GetAsyncKeyState('D') & 0x8000) direction[1] += 1;
	if (GetAsyncKeyState(' ') & 0x8000) jump = true;

	if (!direction[0] && !direction[1] && !direction[2] && !jump) return false;
	sendMovePacket(direction, yaw, pitch, jump);
	return true;
}

// 5) Hunter’s left‑click attack, only for client‑0
void ClientGame::processAttackInput()
{
	if (renderer.currPlayer.playerId != 0) return;   // only hunter
	static bool wasDown = false;
	bool  isDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	if (isDown && !wasDown)            // rising edge
	{
		float pos[3] = {
			renderer.players[0].pos.x,
			renderer.players[0].pos.y,
			renderer.players[0].pos.z
		};
		sendAttackPacket(pos, yaw, pitch);
	}
	wasDown = isDown;
}

void ClientGame::processDodgeInput()
{
	if (renderer.currPlayer.playerId == 0) return;   // hunter cannot dash

	static bool rWasDown = false;
	bool rNowDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

	if (rNowDown && !rWasDown)      // rising edge
		sendDodgePacket();

	rWasDown = rNowDown;
}

void ClientGame::processShopInputs() {
	// if ready, player is locked in and cannot change
	if (ready)
		return;

	// only allow one selection per tick
	if (GetAsyncKeyState('M') & 0x8000) {
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
		sendReadyStatusPacket(selection);
	}
	else if (GetAsyncKeyState('1') & 0x8000) {
		handleShopItemSelection(0);
	}
	else if (GetAsyncKeyState('2') & 0x8000) {
		handleShopItemSelection(1);
	}
	else if (GetAsyncKeyState('3') & 0x8000) {
		handleShopItemSelection(2);
	}
}

void ClientGame::handleShopItemSelection(int choice) {
	ShopItem item = shopOptions[choice];
	int cost = PowerupCosts[item.item];
	if (item.isSelected) 
	{
		item.isSelected = false;
		tempCoins += cost;
	}
	else
	{
		// Only select the item if client has enough coins
		if (item.isBuyable)
		{
			for (int i = 0; i < NUM_POWERUP_OPTIONS; i++)
			{
				shopOptions[i].isSelected = (i == choice);
			}
			tempCoins -= cost;
		}
	}
}

void ClientGame::handleInput()
{
	if (!isWindowFocused()) return;

	switch (appState->gamePhase)
	{
	case GamePhase::START_MENU:
	{
		// Avoid sending multiple ready packets
		if (ready)
			break;

		if (GetAsyncKeyState('M') & 0x8000) {
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