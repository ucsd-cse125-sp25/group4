#include "ServerGame.h"
#include "Parson.h"
using namespace std;
int main() {
    ServerGame server;
    server.readBoundingBoxes();
    while (true) {
        server.update();
    }
}