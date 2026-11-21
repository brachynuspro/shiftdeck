#!/bin/sh
# Renders every ShiftDeck screen to sim/out/*.png (needs clang++ and Pillow).
# TFT_eSPI comes from PlatformIO's libdeps, so run `pio run` once first.
set -e
cd "$(dirname "$0")/.."
TFT=.pio/libdeps/cyd/TFT_eSPI
mkdir -p sim/build sim/out
python3 sim/make_assets.py
clang -c -O1 -w .pio/libdeps/cyd/QRCode/src/qrcode.c -o sim/build/qrcode.o
clang++ -std=gnu++17 -O1 -w -Isim/shim -Isrc -I"$TFT" -I.pio/libdeps/cyd/QRCode/src \
  -DUSER_SETUP_LOADED=1 -include src/User_Setup.h -DTFT_WIDTH=240 -DTFT_HEIGHT=320 -DLOAD_GLCD=1 -DSMOOTH_FONT=1 \
  "$TFT/TFT_eSPI.cpp" src/DisplayUI.cpp src/UIView.cpp src/UIViewManager.cpp \
  src/Views/BootView.cpp src/Views/HomeView.cpp src/Views/SearchView.cpp \
  src/Views/PlaylistView.cpp src/Views/SettingsView.cpp \
  src/Views/SetupView.cpp src/Views/WelcomeView.cpp src/Views/UpdateView.cpp src/Views/AccountView.cpp src/Views/WiFiSetupView.cpp src/Views/SpotifyConfigView.cpp src/Views/SpotifyAuthView.cpp \
  sim/sim.cpp sim/build/qrcode.o -o sim/build/sim
./sim/build/sim sim/assets sim/out
python3 sim/compare.py
