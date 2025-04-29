# Multi-Device Distance Measurement Example

This example creates a single firmware that can be flashed onto multiple DWM1001 devices. Each device measures and reports distances to other devices running the same firmware.

## Overview

The multi-device distance measurement example demonstrates how to use Ultra-Wideband (UWB) technology to measure distances between multiple DWM1001 devices. Unlike the other examples in this repository that require different firmware for initiators and responders, this example uses a single firmware image that can be flashed to all devices.

Each device operates in both initiator and responder modes, taking turns to send poll messages and respond to polls from other devices. The distances between devices are calculated using Single-Sided Two-Way Ranging (SS-TWR) and reported via UART.

## How to Use

1. Set a unique device ID for each device by modifying the `DEVICE_ID` macro in `multi_device_main.c` before building:
   ```c
   #define DEVICE_ID 0  // Set this to 0, 1, or 2 for each of the three devices
   ```

2. Build the firmware for each device with its unique ID:
   - For the first device: Set `DEVICE_ID` to 0, build, and flash
   - For the second device: Set `DEVICE_ID` to 1, build, and flash
   - For the third device: Set `DEVICE_ID` to 2, build, and flash

3. Connect to the UART of each device (115200 baud rate) to see the distance measurements.

## Expected Output

Each device will output something like:

```
Multi-Device Distance Measurement Example
Device ID: 0

Device 0 distances:
  Device 1: 2.34 m
  Device 2: 3.45 m

```

## Implementation Details

- Each device periodically sends poll messages.
- When a device receives a poll from another device, it responds with a message containing timestamps.
- The original polling device calculates the distance based on the round-trip time.
- Devices are identified by their unique ID embedded in the messages.
- The implementation includes timeout detection to handle devices that go out of range or are powered off.

## Modifying the Example

- To add more devices, increase the `MAX_DEVICES` macro value (up to 5 with the current implementation).
- You can adjust the ranging frequency by modifying the `RNG_DELAY_MS` parameter.
- The distance reporting interval can be adjusted in the main task function.

## Troubleshooting

If devices are not detecting each other:
- Make sure each device has a unique ID
- Ensure devices are within range (typically up to 10 meters in indoor environments)
- Check UART connections for debugging information
- Verify that antennas are properly connected 