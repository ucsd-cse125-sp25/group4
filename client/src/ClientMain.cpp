#include "ClientGame.h"
#include <thread>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

    ClientGame client(hInstance, nCmdShow);

    // set up window
    while (true) client.update();
}