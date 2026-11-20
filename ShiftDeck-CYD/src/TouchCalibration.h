/*-------------------------------------------------------------------------------------------------
** TouchCalibration.h - Interactive 2-point touch calibration wizard
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "DisplayUI.h"
#include "XPT2046Touch.h"

// Blocking calibration routine: draws two crosshair targets (top-left and
// bottom-right corners), captures the raw ADC readings while the user
// touches each one, then applies and persists the calibration so touch
// coordinates line up with what's on screen. Runs automatically on first
// boot (see main.cpp) and can be re-triggered manually from Settings.
void runTouchCalibration(DisplayUI &ui, XPT2046Touch &touch);
