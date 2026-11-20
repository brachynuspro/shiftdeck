/*-------------------------------------------------------------------------------------------------
** BootView.cpp - Boot screen implementation (IMGS/inicializacao.png)
** ------------------------------------------------------------------------------------------------
*/
#include "BootView.h"

BootView::BootView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Boot;
}

void BootView::draw() {
    _ui->clear(C_BG);
    drawBrand(CENTER_X - (brandMarkW(30) + _ui->textWidth(APP_NAME, FONT_XL)) / 2, 30, 30, FONT_XL);
    _ui->setTextColor(C_SECONDARY, C_BG);
    _ui->drawCentreString("Inicializando", CENTER_X, _ui->textY(FONT_MD, 57), FONT_MD);

    _knobX = -1;
    _ui->drawSlider(BAR_X, BAR_CY, BAR_W, _progress * 10, C_GREEN, C_TRACK, C_BG, 5, &_knobX);

    _ui->fillRoundRect(PANEL_X, PANEL_Y, PANEL_W, ROW_H * STEP_COUNT + 4, 10, C_CARD, C_BG);
    for (int i = 0; i < STEP_COUNT; i++) drawRow(i, false);

    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawString(VERSION_SHORT, PANEL_X, _ui->textY(FONT_XS, 226), FONT_XS);
    if (_notice) _ui->setTextColor(_noticeColor, C_BG);
    _ui->drawRightString(_notice ? _notice : BOARD_NAME, PANEL_X + PANEL_W, _ui->textY(FONT_XS, 226), FONT_XS);
    _dirty = false;
}

void BootView::drawRow(int i, bool statusOnly) {
    const Row& r = _rows[i];
    int32_t y = PANEL_Y + 2 + i * ROW_H;
    int32_t cy = y + ROW_H / 2;
    int32_t right = PANEL_X + PANEL_W;

    if (!statusOnly) {
        _ui->drawIcon(r.icon, PANEL_X + 18, cy, 15, C_GREEN, C_CARD);
        _ui->setTextColor(C_WHITE, C_CARD);
        _ui->drawString(r.name, PANEL_X + 34, _ui->textY(FONT_SM, cy), FONT_SM);
        if (i > 0) _ui->fillRect(PANEL_X + 10, y, PANEL_W - 20, 1, C_DIVIDER);
    }

    const char* label = r.label;
    uint16_t color = C_MUTED;
    IconID icon = IconID::Spinner;
    switch (r.state) {
        case PENDING: label = label ? label : "Aguardando";    color = C_MUTED;     break;
        case LOADING: label = label ? label : "Carregando..."; color = C_SECONDARY; break;
        case OK:      label = label ? label : "Conectado";     color = C_GREEN;     icon = IconID::CheckCircle; break;
        case FAILED:  label = label ? label : "Erro";          color = C_ERROR;     icon = IconID::Alert; break;
        case TIMEOUT: label = label ? label : "Timeout";       color = C_WARN;      icon = IconID::Alert; break;
    }
    _ui->fillRect(PANEL_X + 110, y + 2, PANEL_W - 112, ROW_H - 3, C_CARD);
    _ui->setTextColor(color, C_CARD);
    _ui->drawRightString(label, right - 30, _ui->textY(FONT_XS, cy), FONT_XS);
    if (r.state != PENDING) _ui->drawIcon(icon, right - 16, cy, 15, color, C_CARD);
}

void BootView::setStep(Step step, State state, const char* label) {
    if (step >= STEP_COUNT) return;
    _rows[step].state = state;
    _rows[step].label = label;
    if (!_dirty) drawRow(step, true);
}

void BootView::setProgress(int pct) {
    _progress = constrain(pct, 0, 100);
    if (!_dirty) _ui->drawSlider(BAR_X, BAR_CY, BAR_W, _progress * 10, C_GREEN, C_TRACK, C_BG, 5, &_knobX);
}

void BootView::animate() {
    if (_dirty || millis() - _lastSpin < 60) return;
    _lastSpin = millis();
    for (int i = 0; i < STEP_COUNT; i++) {
        if (_rows[i].state != LOADING) continue;
        int32_t cy = PANEL_Y + 2 + i * ROW_H + ROW_H / 2;
        int32_t cx = PANEL_X + PANEL_W - 16;
        _ui->fillRect(cx - 9, cy - 9, 19, 19, C_CARD);
        _ui->drawIcon(IconID::Spinner, cx, cy, 15, C_SECONDARY, C_CARD);
    }
}
