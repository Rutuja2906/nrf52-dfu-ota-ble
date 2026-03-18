Firmware Details for Testing and Evaluation

To evaluate and test the developed system for the project work, 11 different firmware versions have been created. These firmwares vary in purpose, size, and signing methods to comprehensively assess system functionality, robustness, and performance.

Summary of Firmwares:

Firmwares 1-4:
Purpose: LED blinking (LED 0 to LED 3).
Size: 200 KB each.

Firmware 5:
Purpose: Similar to Firmwares 1-4 (LED 0 blinking).
Difference: Signed using a different type of signing key (RSA) for robustness testing.
Size: 200 KB.

Firmwares 6-11:
Purpose: LED 0 blinking.
Size Variation: Ranges from 201 KB to 225 KB to analyze the impact of firmware size on transfer performance.

Detailed Firmware Information:

Firmware_1
Function: LED 0 blinking.
Size: 200 KB.

Firmware_2
Function: LED 1 blinking.
Size: 200 KB.

Firmware_3
Function: LED 2 blinking.
Size: 200 KB.

Firmware_4
Function: LED 3 blinking.
Size: 200 KB.

Firmware_5
Function: LED 0 blinking.
Size: 200 KB.
Unique Feature: Signed with RSA key for robustness evaluation.

Firmware_6
Function: LED 0 blinking.
Size: 201 KB.

Firmware_7
Function: LED 0 blinking.
Size: 205 KB.

Firmware_8
Function: LED 0 blinking.
Size: 211 KB.

Firmware_9
Function: LED 0 blinking.
Size: 216 KB.

Firmware_10
Function: LED 0 blinking.
Size: 221 KB.

Firmware_11
Function: LED 0 blinking.
Size: 225 KB.

Firmware_12
Function: A custom Bluetooth Low Energy (BLE) health monitoring service was implemented for live status reporting from a sensor. This included:
GATT Custom Service UUID: 0x181A (Environmental Sensing base, customized)
Characteristic: Health status string (e.g., "OK", "Mild", "Critical", "Sensor Error")
Update Method: Periodic status updates every 15 seconds using a k_work_delayable
Trigger Condition: Only notifies the central if a BLE connection is active
Notification Format: Human-readable UTF-8 strings, allowing easy debugging or mobile app integration

Project Report

Presentation
