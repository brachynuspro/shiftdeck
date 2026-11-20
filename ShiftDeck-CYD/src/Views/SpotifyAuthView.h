/*-------------------------------------------------------------------------------------------------
** SpotifyAuthView.h - Step 3: authorize the account (QR = the device's OAuth page)
**
** The status row updates in place while fetchSpotifyAuthCode() waits.
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "SetupView.h"

class SpotifyAuthView : public SetupView {
public:
    SpotifyAuthView(DisplayUI *ui);
    void setIP(const String& ip);
    void setWaitingSeconds(int s);
    void showConnected();
    void showError(const char* message);

private:
    int _waitingSec = -1;
};
