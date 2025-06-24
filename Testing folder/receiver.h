#pragma once
#include <winsock2.h>

SOCKET setupBluetoothListener(int port);
void acceptBluetoothConnection(SOCKET serverSocket, SOCKET& clientSocket);
void receiveData(SOCKET clientSocket);
void cleanup(SOCKET serverSocket, SOCKET clientSocket);
