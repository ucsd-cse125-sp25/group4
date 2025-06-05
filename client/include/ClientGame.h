#pragma once
#include <WinSock2.h>
#define NOMINMAX
#include <Windows.h>
#include "ClientNetwork.h"
#include "NetworkData.h"
#include "Renderer.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "AudioEngine.h"
#include <string>
using namespace std;


// this links to the directx12 dynamic library
// we should probably set this up in the build system later
#pragma comment(lib,"d3d12.lib") // core DX12
#pragma comment(lib,"dxgi.lib") // more DX12 interfacing stuff
#pragma comment(lib,"D3DCompiler.lib") // shader compiler


class ClientGame {
public:
	ClientGame(HINSTANCE hInstance,  int nCmdShow, string IPAddress);
	~ClientGame(void);

	bool isWindowFocused() const;
	bool isLocalPlayerDead() const;


	void handleInput();
	void processAttackInput();
	void processDodgeInput();
	void processBearInput();
	void processPhantomInput();
	bool processCameraInput();
	bool processMovementInput();
	void processShopInputs();

	void handleSpectatorInput();
	bool processSpectatorCameraInput();
	void processSpectatorKeyboardInput();

	void handleShopItemSelection(int choice);

	void sendDebugPacket(const char*);
	// void sendGameStatePacket(float[4]);
	void sendMovePacket(float[3], float, float, bool);
	void sendCameraPacket(float, float);
	void sendAttackPacket(float origin[3], float yaw, float pitch);
	void sendDodgePacket();
	void sendBearPacket();
	void sendPhantomPacket();
	void sendReadyStatusPacket(uint8_t selection);
	void update();

	GameState* gameState;
	AppState* appState;
	Renderer renderer;

	CAudioEngine* audioEngine;

  ShopOptionsPayload localShopState;

	struct ShopItem {
		Powerup item;
		bool isSelected;
		bool isBuyable;
	};

	ShopItem shopOptions[NUM_POWERUP_OPTIONS];

	bool jumpWasDown = false;
	bool dodgeWasDown = false;
	bool attackWasDown = false;

	AnimationState localAnimState;

private:
	uint64_t instinctExpireTick;
	HWND hwnd;
	int id = -1; // -1 is pre-initialization. 0 should be hunter. 4 should be spectator
	ClientNetwork* network;
	char network_data[MAX_PACKET_SIZE]; //todo this should change once we define the packet sizes

	//camera constants
	float yaw = 0.0;
	float pitch = 0.0;
	static constexpr float MOUSE_SENS = 0.002f;
	static constexpr float ATTACK_RANGE = 10.0f * PLAYER_SCALING_FACTOR;
	bool localDead = false;

	bool ready = false;
	int tempCoins = 0;
	uint8_t powerups[20];

	bool bunnyhop = false; // allow holding jump

	// sounds
	const string a_music = "./SFX/music.wav";
	const string a_jump = "./SFX/jump.wav"; 
	const string a_attack = "./SFX/attack.wav"; 
	const string a_bear = "./SFX/bear_growl.wav";
	const string a_dodge = "./SFX/dodge.wav";
	const string a_move_1 = "./SFX/move_1.wav"; //TODO
	const string a_move_2 = "./SFX/move_2.wav"; // TODO
	const string a_move_3 = "./SFX/move_3.wav"; // TODO
	const string a_move_4 = "./SFX/move_4.wav"; //TODO
	const string a_purchase = "./SFX/purchase.wav";
	const string a_round_end = "./SFX/round_end.wav"; 
	const string a_round_start = "./SFX/round_start.wav"; 
	const string a_darkness = "./SFX/darkness.wav"; //TODO

	void playAudio();
	int bgmChannel;
};
LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wparam, LPARAM lparam);
