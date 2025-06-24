#include <iostream>
#include "receiver.h"

int main() {
    int port = 4;
    SOCKET serverSocket = setupBluetoothListener(port);
    if (serverSocket == INVALID_SOCKET) return 1;

    SOCKET clientSocket = INVALID_SOCKET;
    acceptBluetoothConnection(serverSocket, clientSocket);
    if (clientSocket != INVALID_SOCKET) {
        receiveData(clientSocket);
    }

    cleanup(serverSocket, clientSocket);
    return 0;
}
