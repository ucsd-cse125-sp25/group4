#pragma once
#ifndef UNICODE
#define UNICODE
#endif

// this links to the directx12 dynamic library
// we should probably set this up in the build system later
#pragma comment(lib,"d3d12.lib") // core DX12
#pragma comment(lib,"dxgi.lib") // more DX12 interfacing stuff
#pragma comment(lib,"D3DCompiler.lib") // shader compiler

#include "state.h"
#define NOMINMAX
#include <windows.h>

const wchar_t CLASS_NAME[] = L"Window Class";
const wchar_t GAME_NAME[] = L"$GAME_NAME";

// this function runs every time the window receives a message
LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wparam, LPARAM lparam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

    // Initialize the window class.
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
	State state = {};
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
        &state);

	
	if (!windowHandle) {
		printf("Failed to create window\n");
		return 1;
	}

	if (!state.client_state.renderer.Init(windowHandle)) {
		printf("Failed to initalize renderer\n");
		return 1;
	}

	if (windowHandle == NULL) {
		return 0;
	}

	ShowWindow(windowHandle, nCmdShow);
	
	MSG msg = {};
	// application loop
	while (msg.message != WM_QUIT) {
		// TODO: check for server updates and process them accordingly
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
		}
		else {
			state.client_state.renderer.OnUpdate();
			bool success = state.client_state.renderer.Render(); // render function
		}
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
		// this is NOT called every frame
		state->client_state.renderer.OnUpdate();
		bool success = state->client_state.renderer.Render(); // render function
		// if we want to wait for the next frame, do it here

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