/*-------------------------------------------------------------------------------------------------
** AccountView.h - Shown over Home when Spotify refuses the logged-in account
**
** Development-mode apps only accept the accounts listed in their User Management, and playback
** control needs Premium (SpotifyPlayer::getAccountIssue). Without this screen the buttons just
** did nothing. Same layout as the setup screens; a tap anywhere goes back.
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "SetupView.h"

class AccountView : public SetupView {
public:
    AccountView(DisplayUI *ui);
    void setIssue(int issue);   // SpotifyPlayer::ACCOUNT_NOT_ALLOWED / ACCOUNT_NOT_PREMIUM
    void onTouchDown(uint16_t x, uint16_t y) override { UIView::onTouchDown(x, y); }
    void onTap(uint16_t, uint16_t) override;
};
