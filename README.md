**🚗 Project MicroControl (ESP32-CAM + ATmega128)**
---
**🌐 Web: https://mikanquyt.github.io/MicroControl/**
---
**📖 Project Summary**
---
The FPV Web Controller is a dual-server remote control system that bridges an ESP32-CAM with an ATmega128-V2 microcontroller via a stable 1MHz SPI connection.
---
**🚀 Quick Start Guide**
---
**1. Configure Wi-Fi Credentials**
Update the network variables in the ESP32 source code:

```
C++
const char *ssid = "Your_WiFi_Name"; 
const char *password = "Your_WiFi_Password";
```

**2. Flash and Boot**
Upload the code to your ESP32-CAM using an FTDI programmer.
Open the Serial Monitor (Baud rate 115200).
Wait for the Wi-Fi connection to establish and note the printed local IP address (e.g., http://192.168.43.100).

**3. Operation**
Navigate to the live GitHub Pages link (or your local IP) on a web browser.
Once the camera stream loads, click CONNECT (C) to wake the system and start driving!
