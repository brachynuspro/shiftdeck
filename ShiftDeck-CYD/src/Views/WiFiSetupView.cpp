/*-------------------------------------------------------------------------------------------------
** WiFiSetupView.cpp - Step 1: join the ShiftDeck access point
** ------------------------------------------------------------------------------------------------
*/
#include "WiFiSetupView.h"

WiFiSetupView::WiFiSetupView(DisplayUI *ui)
    : SetupView(ui, 1, "Conecte ao Wi-Fi", "Aponte a câmera do celular para o QR",
                "Ao salvar a rede, a ShiftDeck reinicia sozinha") {
    setIP("192.168.4.1");
}

void WiFiSetupView::setNetwork(const String& ssid, const String& pass) {
    // Canonical Wi-Fi QR: many phones only accept "T:WPA" and need the trailing ";;"
    _qr = "WIFI:T:WPA;S:" + ssid + ";P:" + pass + ";;";
    setRow(0, IconID::Wifi, "Rede", ssid, C_WHITE);
    setRow(1, IconID::Keyboard, "Senha", pass, C_WHITE);
    _dirty = true;
}

void WiFiSetupView::setIP(const String& ip) {
    setRow(2, IconID::Phone, "Se a página não abrir", ip, C_GREEN);
}
