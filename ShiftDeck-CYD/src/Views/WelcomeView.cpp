/*-------------------------------------------------------------------------------------------------
** WelcomeView.cpp - First boot presentation + requirements (see WelcomeView.h)
** ------------------------------------------------------------------------------------------------
*/
#include "WelcomeView.h"

static const int32_t PAD = 14, CARD_Y = 80, ROW_H = 39, BTN_Y = 204, BTN_H = 30;

WelcomeView::WelcomeView(DisplayUI *ui)
    : SetupView(ui, 0, "O que você vai precisar", "Uns 5 minutos. O QR abre o guia passo a passo.", "") {
    _id = ViewID::Welcome;
    _qr = GUIDE_URL;
    setRow(0, IconID::Spotify, "Conta", "Spotify Premium", C_WHITE);
    setRow(1, IconID::Wifi, "Rede", "Wi-Fi 2,4 GHz", C_WHITE);
    setRow(2, IconID::Settings, "Crie de graça", "App no Spotify Dev", C_WHITE);
}

void WelcomeView::draw() {
    if (_page == 0) {
        drawIntro();
    } else {
        SetupView::draw();
        const int32_t w = (SCREEN_W - 2 * PAD - 8) / 2;
        _ui->drawButton(PAD, BTN_Y, w, BTN_H, "Voltar", IconID::Undo, true, C_BUTTON, C_WHITE, C_BG, FONT_MD, 10);
        _ui->drawButton(SCREEN_W - PAD - w, BTN_Y, w, BTN_H, "Começar", IconID::Wifi, true, C_GREEN_DARK, C_WHITE, C_BG, FONT_MD, 10);
    }
    _dirty = false;
}

void WelcomeView::drawIntro() {
    static const struct { IconID icon; const char* title; const char* text; } FEATURES[3] = {
        { IconID::Play,     "Tocando agora",      "Capa, música e controles à vista" },
        { IconID::Playlist, "Playlists e busca",  "Escolha o que tocar sem o celular" },
        { IconID::Speaker,  "Qualquer aparelho",  "Celular, computador ou caixa de som" },
    };
    _ui->clear(C_BG);
    drawBrand(CENTER_X - (brandMarkW(30) + _ui->textWidth(APP_NAME, FONT_XL)) / 2, 30, 30, FONT_XL);
    _ui->setTextColor(C_SECONDARY, C_BG);
    _ui->drawCentreString("Seu Spotify numa tela touch", CENTER_X, _ui->textY(FONT_SM, 61), FONT_SM);

    _ui->fillRoundRect(PAD, CARD_Y, SCREEN_W - 2 * PAD, ROW_H * 3 + 1, 10, C_CARD, C_BG);
    for (int i = 0; i < 3; i++) {
        int32_t y0 = CARD_Y + 1 + i * ROW_H, cy = y0 + ROW_H / 2;
        if (i) _ui->fillRect(PAD + 10, y0 - 1, SCREEN_W - 2 * PAD - 20, 1, C_DIVIDER);
        _ui->drawIcon(FEATURES[i].icon, PAD + 20, cy, 18, C_GREEN, C_CARD);
        _ui->setTextColor(C_WHITE, C_CARD);
        _ui->drawString(FEATURES[i].title, PAD + 40, _ui->textY(FONT_SM_B, cy - 8), FONT_SM_B);
        _ui->setTextColor(C_SECONDARY, C_CARD);
        _ui->drawString(FEATURES[i].text, PAD + 40, _ui->textY(FONT_XS, cy + 9), FONT_XS);
    }
    _ui->drawButton(PAD, BTN_Y, SCREEN_W - 2 * PAD, BTN_H, "Próximo", IconID::ChevronRight, true,
                    C_GREEN_DARK, C_WHITE, C_BG, FONT_MD, 10);
}

void WelcomeView::onTap(uint16_t x, uint16_t y) {
    if (y < BTN_Y - 6) return;
    if (_page == 0)          showPage(1);
    else if (x < CENTER_X)   showPage(0);
    else                     _done = true;
    if (_dirty) draw();
}
