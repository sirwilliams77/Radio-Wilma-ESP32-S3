#pragma once
// Web control panel: http://radio.local (or the IP shown on the screen).
// Station list management, logos, playback control, Wi-Fi, panel password, firmware update.

void webBegin();  // call once Wi-Fi is set up (works in station and access point mode)
void webLoop();   // call from loop()
void webCaptivePortal(bool enable);  // DNS redirect while the setup access point is active
