#include "ClientGame.h"
#include <thread>
#include <string>
#include "InputDialog.h"
using namespace std;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	
    // client.renderer.DBG_DrawCube(XMFLOAT3{ -2, -1, -1 }, XMFLOAT3{ 2, 1, 1 });
    // client.renderer.DBG_DrawCube(XMFLOAT3{ 0, -3, -1 }, XMFLOAT3{ 1, 1, 1 });
	// Ask the user for a name (or whatever)
	std::string input = ShowInputDialog(
		hInstance,
		L"Enter IP address of the server", 300, 120
	);
	if (!input.empty()) {
		// convert wstring to char*
		// User clicked OK; do something with input
		ClientGame client(hInstance, nCmdShow, input);
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
	else {
		// User cancelled
		return 0;
	}
}
