#include "ClientGame.h"

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
}

void ClientGame::sendDebugPacket(uint64_t tick) {
	DebugPayload dbg{};
	sprintf_s(dbg.message, sizeof(dbg.message), "debug: tick %llu", tick);

	char packet_data[HDR_SIZE + sizeof(DebugPayload)];
	NetworkServices::buildPacket<DebugPayload>(PacketType::DEBUG, dbg, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(DebugPayload));
}

void ClientGame::sendGameStatePacket(float posDelta[4]) {
	GameState* state = new GameState{ {0} };
	//sprintf_s(state.position, sizeof(state.position), "debug: tick %llu", tick);
	for (int i = 0; i < 4; i++) {
		state->position[i] = posDelta[i];
	}

	char packet_data[HDR_SIZE + sizeof(GameState)];
	NetworkServices::buildPacket<GameState>(PacketType::MOVE, *state, packet_data);
	NetworkServices::sendMessage(network->ConnectSocket, packet_data, HDR_SIZE + sizeof(DebugPayload));
	delete state;
}

void ClientGame::update() {
	int len = network->receivePackets(network_data);
	if (len > 0) {
		// here, network_data should contain the game state packet
		PacketHeader* hdr = (PacketHeader*)network_data;
		switch (hdr->type) {
		case PacketType::GAME_STATE: 
		{
			//GameStatePayload* game_state = (GameStatePayload*)(network_data + HDR_SIZE);
			//printf("received update for tick %llu \n", game_state->tick);
			//// add logic here
			//if (game_state->tick % (uint64_t)64 == 0) {
			//	sendDebugPacket(game_state->tick);
			//}
			GameState* state = (GameState*)(network_data + HDR_SIZE);
			char msgbuf[1000];
			// printf(msgbuf, "Packet received y=%f \n", state->position[1]);

			renderer.m_constantBufferData.offset.y = state->position[1];

			break;
		}
		case PacketType::DEBUG: 
		{
			break;
		}
		default:
			// printf("error in packet type %d, expected GAME_STATE or DEBUG\n", hdr->type);
			break;
		}

	}


	
	// ---------------------------------------------------------------	
	// Client Input Handling 

	float positionDelta[4] = {};
	bool isUpdate = false;

	if (GetKeyState('W') & 0x8000) {
		positionDelta[1] += 0.015f;
		isUpdate = true;
	}
	else if (GetKeyState('S') & 0x8000) {
		positionDelta[1] -= 0.015f;
		isUpdate = true;
	}

	if (isUpdate) {
		sendGameStatePacket(positionDelta);
	}


	// TODO: check for server updates and process them accordingly

	// ---------------------------------------------------------------	
	// Update GPU data and render 
	// copy new data to the GPU
	renderer.OnUpdate();
	// render the frame
	// this will block if 2 frames have been sent to the GPU and none have been drawn 
	bool success = renderer.Render(); // render function

}

ClientGame::~ClientGame() {
	delete network;
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
	{
		// TODO: handle closing the window	
	}
	break;
	} 

	return DefWindowProc(window_handle, uMsg, wParam, lParam);
} 