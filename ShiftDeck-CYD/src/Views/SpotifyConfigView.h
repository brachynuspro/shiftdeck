/*-------------------------------------------------------------------------------------------------
** SpotifyConfigView.h - Step 2: enter the Spotify app's Client ID (QR = config page)
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "SetupView.h"

class SpotifyConfigView : public SetupView {
public:
    SpotifyConfigView(DisplayUI *ui);
    void setIP(const String& ip);
};
