# Smart BB-8 Inspired Robot

An advanced, IoT-enabled, modular BB-8 inspired spherical mobile robot that combines remote-controlled differential drive movement, an interactive magnetic detachable head, animated electronic dual-OLED eyes, an onboard ESP32-CAM for still-image capture, and a real-time web monitoring dashboard.

---

## System Overview

The robot is divided into four coordinated subsystems working together over a shared wireless network, backed by a real-time browser dashboard:
1. **The Rolling Robot Body:** Handles physical locomotion via internal differential drive motors, orientation sensing (MPU6050), battery monitoring, and magnetic activation.
2. **The Smart Head:** Features dual OLED animated eyes, head-attachment magnetic sensing, and an onboard camera for live streaming and still captures.
3. **The Handheld Wireless Remote:** Provides dual joysticks for proportional control and four dedicated push buttons (PB1–PB4) for smart functions, complete with an OLED status display and continuous fail-safe heartbeat protection.
4. **The Live Web Dashboard:** Provides a persistent, dynamic browser-based monitoring and secondary control interface.

---

## Main Project Features

| Feature | Description |
| :--- | :--- |
| **BB-8 Style Spherical Movement** | Internal differential drive enables smooth forward, reverse, and turning maneuvers. |
| **Dedicated Wireless Remote** | Dual joysticks deliver proportional control, supported by four dedicated function buttons. |
| **Animated Dual OLED Eyes** | Dual electronic displays render distinct operating-mode and blink animations. |
| **Eye Blink (PB1)** | Triggers an immediate synchronized eye-blink animation across both displays. |
| **Eye Attach Mode (PB2)** | Toggles a dedicated visual attachment/interaction eye mode. |
| **Smart Detachable Head** | Magnetic coupling arrangement keeps the head positioned above the rolling body. |
| **Head Attachment Detection** | Reed sensor logic reports real-time `HEAD ATTACHED` or `HEAD DETACHED` status. |
| **Onboard Camera** | ESP32-CAM captures still images triggered by remote command PB3 or the dashboard. |
| **Controlled Reset (PB4)** | Safely commands motor outputs to stop before executing a system reset sequence. |
| **Orientation Monitoring** | MPU6050 tracks and streams live pitch, roll, and motion data. |
| **Battery Monitoring** | Continuous voltage tracking generates active low-battery warnings. |
| **Magnetic Body Activation** | Magnetic reed detection acts as the electronic power-enable condition for body electronics. |
| **Remote OLED Telemetry** | Displays vital robot status indicators directly on the handheld controller. |
| **Live Web Dashboard** | Real-time browser UI for movement, orientation, battery, eyes, and camera tracking. |
| **Communication Fail-Safe** | Automatically halts motor movement if valid remote wireless packets are lost. |

---


## Safety & Reliability Architecture

* **Default Safe States:** Movement defaults to a `STOP` state during startup, system resets, and communication dropouts.
* **Current Isolation:** Motor current is managed strictly through dedicated motor drivers rather than routing through sensor contacts. Magnetic reed switches operate solely as low-current digital sense inputs.
* **Motion Smoothing:** Joystick inputs incorporate dead-zone filtering and