#pragma once
#include <winsock2.h>
#include <string>

// Sends data over an open Bluetooth socket
bool sendDataOverBluetooth(SOCKET sock, const std::string& data);
