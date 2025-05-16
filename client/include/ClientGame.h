#pragma once
#include <WinSock2.h>
#define NOMINMAX
#include <Windows.h>
#include "ClientNetwork.h"
#include "NetworkData.h"
#include "Renderer.h"


// this links to the directx12 dynamic library
// we should probably set this up in the build system later
#pragma comment(lib,"d3d12.lib") // core DX12
#pragma comment(lib,"dxgi.lib") // more DX12 interfacing stuff
#pragma comment(lib,"D3DCompiler.lib") // shader compiler


class ClientGame {
public:
	ClientGame(HINSTANCE hInstance,  int nCmdShow);
	~ClientGame(void);

	bool isWindowFocused() const;
	bool isLocalPlayerDead() const;
	bool processCameraInput();
	bool processMovementInput();

	void sendDebugPacket(const char*);
	// void sendGameStatePacket(float[4]);
	void sendMovePacket(float[3], float, float, bool);
	void sendCameraPacket(float, float);
	void sendAttackPacket(float origin[3], float yaw, float pitch);
	void sendDodgePacket();
	void sendStartMenuStatusPacket();
	void update();
	void processAttackInput();
	void processDodgeInput();
	void handleInput();

	GameState* gameState;
	AppState* appState;
	Renderer renderer;
private:
	HWND hwnd;
	unsigned int id;
	ClientNetwork* network;
	char network_data[MAX_PACKET_SIZE]; //todo this should change once we define the packet sizes

	//camera constants
	float yaw = 0.0;
	float pitch = 0.0;
	static constexpr float MOUSE_SENS = 0.002f;
	static constexpr float ATTACK_RANGE = 4.0f;
	bool localDead = false;
};
LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wparam, LPARAM lparam);
