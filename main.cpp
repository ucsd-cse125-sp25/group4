#pragma once
#ifndef UNICODE
#define UNICODE
#endif

// this links to the directx12 dynamic library
// we should probably set this up in the build system later
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#include "state.h"
#define NOMINMAX
#include <windows.h>

const wchar_t CLASS_NAME[] = L"Window Class";
const wchar_t GAME_NAME[] = L"$GAME_NAME";

// this function runs every time the window receives a message
LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wparam, LPARAM lparam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	WNDCLASS window_class = {};

	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = hInstance;
	window_class.lpszClassName = CLASS_NAME; 
	
	// register window class to operating system
	RegisterClass(&window_class);
	
	// initialize the state
	State state = {};
	HWND window_handle = CreateWindowEx(
		0,
		CLASS_NAME,
		GAME_NAME,
		WS_OVERLAPPEDWINDOW, // window style; may need to be changed later because this has a title bar and system menu
		// size and position
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,

		NULL, // parent window
		NULL, // hMenu
		hInstance, // instance handle
		&state // application data
		);

	if (!state.client_state.renderer.Init(window_handle)) {
		printf("Failed to initalize renderer\n");
		return 1;
	}

	if (window_handle == NULL) {
		return 0;
	}

	ShowWindow(window_handle, nCmdShow);
	
	MSG msg;
	// application loop
	while (GetMessage(&msg, NULL, 0, 0) != 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

inline State *GetState(HWND window_handle) {
	LONG_PTR ptr = GetWindowLongPtr(window_handle, GWLP_USERDATA);
	State *state = reinterpret_cast<State *>(ptr);
	return state;
}


LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	State *state;
	if (uMsg == WM_CREATE) {
		CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
		state = reinterpret_cast<State *>(pCreate->lpCreateParams);
		SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR)state);
	}
	else {
		state = GetState(window_handle);
	}
	switch (uMsg) { // send messages to the server here
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
		bool success = state->client_state.renderer.Render();
	}
	break;
	case WM_CLOSE:
	{
		
	}
	break;
	} 

	return DefWindowProc(window_handle, uMsg, wParam, lParam);
} 