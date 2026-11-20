/*-------------------------------------------------------------------------------------------------
** WiFiSetupView.h - Step 1: join the ShiftDeck access point (QR = Wi-Fi credentials)
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "SetupView.h"

class WiFiSetupView : public SetupView {
public:
    WiFiSetupView(DisplayUI *ui);
    void setNetwork(const String& ssid, const String& pass);
    void setIP(const String& ip);
};
