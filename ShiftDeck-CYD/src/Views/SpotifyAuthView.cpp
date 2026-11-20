/*-------------------------------------------------------------------------------------------------
** SpotifyAuthView.cpp - Step 3: authorize the account
** ------------------------------------------------------------------------------------------------
*/
#include "SpotifyAuthView.h"

SpotifyAuthView::SpotifyAuthView(DisplayUI *ui)
    : SetupView(ui, 3, "Autorize o Spotify", "Último passo: libere o acesso à sua conta",
                "Sem resposta em 5 min, a ShiftDeck reinicia") {
    setRow(1, IconID::Spotify, "Na página, toque em", "Autorizar Spotify", C_WHITE);
    setRow(2, IconID::Spinner, "Status", "Aguardando...", C_SECONDARY);
    setIP("192.168.0.1");
}

void SpotifyAuthView::setIP(const String& ip) {
    _qr = "http://" + ip;
    setRow(0, IconID::Phone, "Abra no celular", ip, C_GREEN);
    _dirty = true;
}

void SpotifyAuthView::setWaitingSeconds(int s) {
    if (s == _waitingSec) return;
    _waitingSec = s;
    setRow(2, IconID::Spinner, "Status", "Aguardando " + String(s) + "s", C_SECONDARY);
}

void SpotifyAuthView::showConnected() {
    setRow(2, IconID::CheckCircle, "Status", "Conectado!", C_GREEN);
}

void SpotifyAuthView::showError(const char* message) {
    setRow(2, IconID::Alert, "Erro, reiniciando...", message, C_ERROR);
}
