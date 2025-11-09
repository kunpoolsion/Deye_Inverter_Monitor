Simple DEYE solar system monitor.

Two versions:
  - Only web interface: any esp32 (to be uploaded). 
  - LCD + Web interface: made for Waveshare ESP32-S3-Touch-LCD-7 Display board.

You can easily mod for any other board with esp32 with psram, ST7262 LCD and GT911 Touch drivers changing pins on waveshare library.
For any other combination of 800x480 LCD/Touch driver you can use the Expressif ESP32_Display_Panel library.
Based on LVGL 8.3.11 (limited by Waveshare library)

Independent and only wifi needed.

Tested on Deye Hybrid Inverters with Solarman wifi adapter.
For any other Deye inverter (or any other inverter using solarmanv5 adapter) you can adapt modbus registers on SolarmanV5.cpp

Only spanish version ATM.

Integrated Solarmanv5 protocol (ported from pysolarmanv5 and HomeAssistant descriptors).
WifiAP if no connection to change configuration (for lazy people that don´t wanna fight with compilation).

Some libraries were modded, use them to avoid compilation errors!!!
Flasheable BINs included for LCD version.

ENJOY IT!

7" Inch Display:
![screen](https://github.com/user-attachments/assets/f3f6f1b4-889a-427d-9bbe-ad281676aaba)

PC Web Interface:
<img width="1673" height="674" alt="web_interface" src="https://github.com/user-attachments/assets/32f06721-b020-462c-995a-96069dab9d49" />

Mobile Web Interface:
![mobile_web](https://github.com/user-attachments/assets/c0fefa4c-32c6-4546-b1c2-73415209c53b)
