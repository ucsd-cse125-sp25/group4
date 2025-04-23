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

	void sendDebugPacket(uint64_t);
	void update();

	GameState gameState;
	Renderer renderer;
private:
	ClientNetwork* network;
	char network_data[MAX_PACKET_SIZE];
};
LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wparam, LPARAM lparam);