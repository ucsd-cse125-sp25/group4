#pragma once
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>

const wchar_t CLASS_NAME[] = L"Window Class";
const wchar_t GAME_NAME[] = L"$GAME_NAME";

// this function runs every time the window receives as message
LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wparam, LPARAM lparam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	WNDCLASS window_class = {};

	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = hInstance;
	window_class.lpszClassName = CLASS_NAME; 
	
	// register window class to operating system
	RegisterClass(&window_class);

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
		NULL // additional application data
		);

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


LRESULT CALLBACK WindowProc(HWND window_handle, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
	} 

	return DefWindowProc(window_handle, uMsg, wParam, lParam);
} 