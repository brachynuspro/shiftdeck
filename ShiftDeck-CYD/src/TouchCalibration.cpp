/*-------------------------------------------------------------------------------------------------
** TouchCalibration.cpp - Interactive 2-point touch calibration wizard
** ------------------------------------------------------------------------------------------------
*/
#include "TouchCalibration.h"
#include "Vault.h"
#include "config.h"
#include "logTags.h"
#include "SCLogger.h"

// Waits for the panel to report a touch (with a generous timeout so we can
// never hang forever in the car if the panel is somehow not responding),
// samples several raw ADC readings for stability, then waits for release
// before returning so the same touch isn't captured twice.
static void captureCalibrationPoint(DisplayUI &ui, XPT2046Touch &touch,
                                     int32_t screenX, int32_t screenY,
                                     int16_t &rawX, int16_t &rawY) {
    ui.clear(C_BG);
    ui.setTextColor(C_SECONDARY, C_BG);
    ui.drawCentreString("Toque no alvo com precisão", CENTER_X, 20, FONT_SM);
    ui.drawCrosshair(screenX, screenY, 14, C_GREEN);

    unsigned long start = millis();
    while (!touch.touched() && millis() - start < 30000) {
        delay(10);
    }

    touch.getRawPoint(rawX, rawY, 10);

    ui.drawCrosshair(screenX, screenY, 14, C_BG); // erase
    ui.fillRect(screenX - 16, screenY - 16, 32, 32, C_BG);

    // Wait for release so the next target isn't captured by the same touch.
    while (touch.touched()) {
        delay(10);
    }
    delay(150); // debounce
}

void runTouchCalibration(DisplayUI &ui, XPT2046Touch &touch) {
    spLogI(LOGTAG_INPUT, "Starting touch calibration");

    ui.clear(C_BG);
    ui.setTextColor(C_GREEN, C_BG);
    ui.drawCentreString("Calibração do toque", CENTER_X, 60, FONT_SM);
    ui.setTextColor(C_SECONDARY, C_BG);
    ui.drawCentreString("Toque nos 2 alvos que vão aparecer", CENTER_X, 100, FONT_SM);
    delay(1800);

    int16_t rawX1, rawY1, rawX2, rawY2;

    captureCalibrationPoint(ui, touch, TOUCH_CAL_MARGIN, TOUCH_CAL_MARGIN, rawX1, rawY1);
    captureCalibrationPoint(ui, touch, SCREEN_W - TOUCH_CAL_MARGIN, SCREEN_H - TOUCH_CAL_MARGIN, rawX2, rawY2);

    touch.setCalibration(rawX1, rawY1, rawX2, rawY2);
    Vault::getInstance().setTouchCalibration(rawX1, rawY1, rawX2, rawY2);

    ui.clear(C_BG);
    ui.setTextColor(C_GREEN, C_BG);
    ui.drawCentreString("Calibração concluída!", CENTER_X, CENTER_Y, FONT_SM);
    delay(1200);

    spLogI(LOGTAG_INPUT, "Touch calibration saved: raw1=(%d,%d) raw2=(%d,%d)",
           rawX1, rawY1, rawX2, rawY2);
}
