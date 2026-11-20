/*-------------------------------------------------------------------------------------------------
** UpdateView.h - "Nova versão disponível": asks, then shows the download until the reboot
**
** Opened from Home when the player task finds a newer version (UM_UPDATE_STATUS), or from
** Configurações › Atualização. The download runs on the player task (Updater.h) and reports
** through UM_UPDATE_PROGRESS.
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "UIView.h"

class UpdateView : public UIView {
public:
    UpdateView(DisplayUI *ui);
    void draw() override;
    void onTap(uint16_t x, uint16_t y) override;
    void onEnter() override;
    void handleMessage(SCUIMessage *msg) override;

    // Simulator hook: render a state without the network.
    void preview(const char* latest, int pct, const char* error = nullptr);

private:
    enum State : uint8_t { PROMPT, INSTALLING, FAILED, RESTARTING };
    State _state = PROMPT;
    String _latest, _error;
    int _pct = 0;
    int16_t _knobX = -1;
    void drawProgress();
    void startInstall();
};
