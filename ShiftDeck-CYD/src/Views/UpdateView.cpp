/*-------------------------------------------------------------------------------------------------
** UpdateView.cpp - Firmware update prompt and progress (see UpdateView.h)
** ------------------------------------------------------------------------------------------------
*/
#include "UpdateView.h"
#include "SpotifyPlayer.h"
#include "UIViewManager.h"

static const int32_t PAD = 14, BTN_Y = 202, BTN_H = 30, BAR_X = 40, BAR_W = 240, BAR_CY = 168;

UpdateView::UpdateView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Update;
}

void UpdateView::onEnter() {
    _state = PROMPT;
    _latest = SpotifyPlayer::getInstance().getLatestVersion();
    _dirty = true;
}

void UpdateView::preview(const char* latest, int pct, const char* error) {
    _latest = latest;
    _pct = pct;
    _error = error ? error : "";
    _state = error ? FAILED : pct >= 100 ? RESTARTING : pct >= 0 ? INSTALLING : PROMPT;
    _dirty = true;
}

void UpdateView::draw() {
    _ui->clear(C_BG);
    drawBrand(PAD, 18, 18, FONT_MD_B);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawRightString((String("v") + VERSION).c_str(), SCREEN_W - PAD, _ui->textY(FONT_XS, 18), FONT_XS);

    struct { IconID icon; uint16_t color; const char* title; String sub; const char* note; } c;
    switch (_state) {
        case PROMPT:
            c = { IconID::Update, C_GREEN, "Nova versão disponível",
                  String("v") + VERSION + "  para  v" + _latest,
                  "Leva 1 minuto. Wi-Fi e conta ficam salvos." };
            break;
        case INSTALLING:
            c = { IconID::Update, C_SECONDARY, "Atualizando...", "Não desligue a ShiftDeck", "" };
            break;
        case RESTARTING:
            c = { IconID::CheckCircle, C_GREEN, "Pronto!", "Reiniciando na versão nova...", "" };
            break;
        case FAILED:
            c = { IconID::Alert, C_ERROR, "Não foi possível atualizar", _error,
                  "Nada mudou: a versão atual continua instalada." };
            break;
    }
    _ui->drawIcon(c.icon, CENTER_X, 70, 40, c.color, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawCentreString(c.title, CENTER_X, _ui->textY(FONT_LG, 112), FONT_LG);
    _ui->setTextColor(_state == FAILED ? C_ERROR : C_SECONDARY, C_BG);
    _ui->drawCentreString(_ui->fitText(c.sub, SCREEN_W - 2 * PAD, FONT_MD).c_str(), CENTER_X, _ui->textY(FONT_MD, 138), FONT_MD);

    if (_state == INSTALLING) {
        _knobX = -1;
        drawProgress();
    } else {
        _ui->setTextColor(C_MUTED, C_BG);
        _ui->drawCentreString(c.note, CENTER_X, _ui->textY(FONT_XS, 166), FONT_XS);
    }
    if (_state == PROMPT || _state == FAILED) {
        const int32_t w = (SCREEN_W - 2 * PAD - 8) / 2;
        _ui->drawButton(PAD, BTN_Y, w, BTN_H, _state == PROMPT ? "Depois" : "Voltar", IconID::Undo, true,
                        C_BUTTON, C_WHITE, C_BG, FONT_MD, 10);
        _ui->drawButton(SCREEN_W - PAD - w, BTN_Y, w, BTN_H, _state == PROMPT ? "Atualizar" : "Tentar de novo",
                        IconID::Update, true, C_GREEN_DARK, C_WHITE, C_BG, FONT_MD, 10);
    }
    _dirty = false;
}

void UpdateView::drawProgress() {
    _ui->drawSlider(BAR_X, BAR_CY, BAR_W, _pct * 10, C_GREEN, C_TRACK, C_BG, 6, &_knobX);
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", _pct);
    _ui->fillRect(CENTER_X - 30, BAR_CY + 12, 60, 22, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawCentreString(pct, CENTER_X, _ui->textY(FONT_MD_B, BAR_CY + 22), FONT_MD_B);
}

void UpdateView::startInstall() {
    _state = INSTALLING;
    _pct = 0;
    draw();
    SpotifyPlayer::getInstance().installUpdate();
}

void UpdateView::onTap(uint16_t x, uint16_t y) {
    if ((_state != PROMPT && _state != FAILED) || y < BTN_Y - 6) return;
    if (x < CENTER_X) UIViewManager::getInstance().back();
    else              startInstall();
}

void UpdateView::handleMessage(SCUIMessage *msg) {
    if (msg->type != SCUIMessageType::UM_UPDATE_PROGRESS) {
        UIView::handleMessage(msg);
        return;
    }
    if (msg->num < 0) {
        _state = FAILED;
        _error = msg->str;
        _dirty = true;
    } else if (msg->num >= 100) {
        _state = RESTARTING;
        _dirty = true;
    } else if (_state == INSTALLING && msg->num != _pct) {
        _pct = msg->num;
        if (!_dirty) drawProgress();
    }
}
