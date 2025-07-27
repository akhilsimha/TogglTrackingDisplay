# Toggl Timer Display

So my office uses Toggl to track the working hours. I always used to check in Toggl Website if I have met the hours or not. And one day one of my colleague suggested that I can use API instead and that was such a good idea. So I checked for components in my room and found the perfect set: an ESP32 Wroom and a TFT Display. 


## How to use the project for your personal setup:
1. Open the `.ino` file in Arduino IDE. 
2. Go to File/Preferences and set Sketchbook location to the path of the UI project (where this README is located)
3. Go to `libraries/TFT_eSPI` and open `User_Setup.h` or `User_Setup_Select.h` with a text editor to configure pins for your display.
4. Select your board (install if needed)
5. Build the project
