#pragma once
#include <winsock2.h>


SOCKET createBluetoothSocket(ULONGLONG btAddress, int port);


void closeBluetoothSocket(SOCKET sock);
