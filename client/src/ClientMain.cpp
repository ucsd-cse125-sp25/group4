#include "ClientGame.h"
#include <thread>

int main() {
    ClientGame client;
    while (true) client.update();
}