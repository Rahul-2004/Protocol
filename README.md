# Cross-PC Input Relay System

Windows cross-machine input relay using Raw Input capture on PC1, Bluetooth RFCOMM transport, `SendInput` injection on PC2, and a compact binary protocol with health metrics.

## Implemented Features

- Windows Raw Input capture uses `RegisterRawInputDevices` with `RIDEV_INPUTSINK` for mouse and keyboard events.
- Local PC1 input is not disrupted because the app does not install a low-level swallowing hook and does not use `RIDEV_NOLEGACY`.
- Bluetooth Classic RFCOMM transport sends fixed 20-byte binary packets with exact-send/exact-receive handling.
- Protocol supports `HELLO`, `ACK`, `TOGGLE_ON`, `TOGGLE_OFF`, `INPUT_EVENT`, `HEARTBEAT`, and `FLUSH_SEQ` packets.
- Reliability features include XOR checksum validation, monotonic sequence numbers, stale/duplicate drops, source IDs, and receiver-side loop prevention.
- Five middle-button clicks within 1.5 seconds toggles PC1 between local and forwarding modes, with `TOGGLE_ON` retry and ACK handling.
- Heartbeats are sent every 5 seconds while forwarding; PC2 reverts to local control after a 15-second heartbeat timeout.
- PC2 injects mouse movement, button, wheel, and keyboard packets using Windows `SendInput`.
- Console observability dashboard displays connection state, forwarding state, sent/received/dropped packets, sequence gaps as packet loss, reconnect attempts, heartbeat age, latency samples, and recent reconnect/toggle events.

## Build

Use Windows with Visual Studio Build Tools or MinGW that provides Windows Bluetooth headers/libraries.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The portable protocol self-test can also build on non-Windows hosts:

```bash
cmake -S . -B build
cmake --build build --target protocol_self_test
ctest --test-dir build --output-on-failure
```

## Run

Start PC2 first so it listens for the Bluetooth RFCOMM connection:

```powershell
.\build\Release\input_relay.exe receiver 4
```

Start PC1 with PC2's Bluetooth address and the same RFCOMM port:

```powershell
.\build\Release\input_relay.exe sender 00:11:22:33:44:55 4
```

Click the middle mouse button five times within 1.5 seconds on PC1 to toggle forwarding on or off.

## Source Map

- `main.cpp`: sender/receiver app, Raw Input capture, state machine, toggle logic, heartbeat/reconnect handling.
- `Protocol/MousePacket.*`: 20-byte binary protocol, serialization, checksum, sequencing, loop-prevention decisions.
- `Protocol/BluetoothTransport.*`: Windows Bluetooth RFCOMM transport wrapper.
- `Protocol/Observability.*`: live console dashboard and metrics.
- `input_injection/MouseInjector.*`: Windows `SendInput` packet injection.
- `input_capture/MouseHook.*`: legacy-named Raw Input sample; no `WH_MOUSE_LL` hook is used.
- `tests/protocol_self_test.cpp`: portable protocol behavior test.
