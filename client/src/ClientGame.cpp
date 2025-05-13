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
    RECT windowRect = { 0, 0, 1920, 1080};
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

void ClientGame::sendMovePacket(char dir, float yaw, float pitch) {
	MovePayload mv{};
	mv.direction = dir;
	mv.yaw = yaw;
	mv.pitch = pitch;

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

// inside ClientGame ------------------------------------------------
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
			GameState* state = (GameState*)(network_data + HDR_SIZE);
			char msgbuf[1000];
			// printf(msgbuf, "Packet received y=%f \n", state->position[1]);

			for (int i = 0; i < 4; i++) {
				renderer.players[i].pos.x = state->players[i].x;
				renderer.players[i].pos.y = state->players[i].y;
				renderer.players[i].pos.z = state->players[i].z;

				// update the rotation from other players only.
				if (i == renderer.currPlayer.playerId) continue;
				renderer.players[i].lookDir.pitch = state->players[i].pitch;
				renderer.players[i].lookDir.yaw = state->players[i].yaw;
			}

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

void ClientGame::handleInput() {
	// if game window is not focused, don't register input
	if (GetForegroundWindow() != hwnd) 
		return;

	bool movUpdate = false;
	char dir;

	if (GetAsyncKeyState('W') & 0x8000) {
		dir = 'W';
		movUpdate = true;
	}
	if (GetAsyncKeyState('S') & 0x8000) {
		dir = 'S';
		movUpdate = true;
	}
	if (GetAsyncKeyState('A') & 0x8000) {
		dir = 'A';
		movUpdate = true;
	}
	if (GetAsyncKeyState('D') & 0x8000) {
		dir = 'D';
		movUpdate = true;
	}


	// CAMERA LOGIC:
	bool camUpdate = false;
	// current cursor position
	POINT p;
	GetCursorPos(&p); 
	// center of client
	RECT rc;
	GetClientRect(hwnd, &rc);
	POINT centre{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
	ClientToScreen(hwnd, &centre); // get client's center pos relative to screen

	int dx = p.x - centre.x;
	int dy = p.y - centre.y;
	if (dx != 0 || dy != 0) {
		camUpdate = true;
		yaw += -dx * MOUSE_SENS;            
		pitch += dy * MOUSE_SENS;      
		pitch = std::clamp(pitch,
						   XMConvertToRadians(-89.0f),
			               XMConvertToRadians(+89.0f));
		// recenter cursor
		if(camUpdate) SetCursorPos(centre.x, centre.y);
	}

	if (camUpdate) {
		sendCameraPacket(yaw, pitch);

		// update own's camera
		renderer.players[renderer.currPlayer.playerId].lookDir.pitch = pitch;
		renderer.players[renderer.currPlayer.playerId].lookDir.yaw = yaw;
	}
	if (movUpdate) {
		sendMovePacket(dir, yaw, pitch);
	}

	// Attack logic
	static bool wasPressedLastFrame = false;
	bool isPressedNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	if (isPressedNow && !wasPressedLastFrame) {          // rising edge only
		// player’s current world position
		float pos[3] = {
			renderer.players[renderer.currPlayer.playerId].pos.x,
			renderer.players[renderer.currPlayer.playerId].pos.y,
			renderer.players[renderer.currPlayer.playerId].pos.z
		};
		sendAttackPacket(pos, yaw, pitch);
	}
	wasPressedLastFrame = isPressedNow;
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