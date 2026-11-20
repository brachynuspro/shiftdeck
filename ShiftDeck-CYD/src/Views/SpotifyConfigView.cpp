/*-------------------------------------------------------------------------------------------------
** SpotifyConfigView.cpp - Step 2: enter the Spotify app's Client ID / Secret
** ------------------------------------------------------------------------------------------------
*/
#include "SpotifyConfigView.h"

SpotifyConfigView::SpotifyConfigView(DisplayUI *ui)
    : SetupView(ui, 2, "Conecte o Spotify", "Celular na mesma rede Wi-Fi da ShiftDeck",
                "Ao salvar, a ShiftDeck reinicia sozinha") {
    setRow(1, IconID::Spotify, "Cole do seu app Spotify", "Client ID e Secret", C_WHITE);
    setRow(2, IconID::Settings, "Ainda não tem o app?", "O link está na página", C_WHITE);
    setIP("192.168.0.1");
}

void SpotifyConfigView::setIP(const String& ip) {
    _qr = "http://" + ip;
    setRow(0, IconID::Phone, "Abra no celular", ip, C_GREEN);
    _dirty = true;
}
