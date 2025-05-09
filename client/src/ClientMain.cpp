#include "ClientGame.h"
#include <thread>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

    ClientGame client(hInstance, nCmdShow);
	
    client.renderer.DBG_DrawCube(XMFLOAT3{ -2, -1, -1 }, XMFLOAT3{ 2, 1, 1 });
    client.renderer.DBG_DrawCube(XMFLOAT3{ 0, -3, -1 }, XMFLOAT3{ 1, 1, 1 });
    // set up window
	MSG msg = {};
	// application loop
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			client.update();
		}
	}
}
