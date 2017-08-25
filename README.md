# ESP32_VGM_Player_YM2612_SN76489

A video game music player based on the ESP32

This is yet another follow-up to my Sega Genesis / Master System hardware video game player. This version will feature SD card support and smooth playback speeds, even for PCM rich songs.

You can find the predecessor to this project here: https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612

# Update to this project
Due to shift-register inconsistancy errors and the complexities involved with voltage translation, the ESP-32 version of this project will be put on hold indefinitely. This isn't the end though - I plan to explore other microcontrollers that are native 5v tolerant. The ESP32 just isn't as well-supported as it needs to be yet.
Code-wise, this repository seems to work fine, so I'll keep it up for future reference. 
I reccomend visiting the previous project based on the ESP8266 for now: https://github.com/AidanHockey5/ESP8266_VGM_Player_PSG_YM2612

The direct new successor to the ESP8266 version of this project will now be based on the Teensy 3.5. Initial tests have been very successful! https://github.com/AidanHockey5/Teensy_3_5_VGM_Player_YM2612_SN76489
