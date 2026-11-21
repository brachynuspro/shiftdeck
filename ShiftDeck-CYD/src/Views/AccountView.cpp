/*-------------------------------------------------------------------------------------------------
** AccountView.cpp - Account refused by Spotify (see AccountView.h)
** ------------------------------------------------------------------------------------------------
*/
#include "AccountView.h"
#include "UIViewManager.h"
#include "SpotifyPlayer.h"

AccountView::AccountView(DisplayUI *ui) : SetupView(ui, 0, "", "", "") {
    _id = ViewID::Account;
    setIssue(SpotifyPlayer::ACCOUNT_NOT_ALLOWED);
}

void AccountView::setIssue(int issue) {
    if (issue == SpotifyPlayer::ACCOUNT_NOT_PREMIUM) {
        setHeader(0, "Precisa do Premium", "O Spotify só deixa controlar com conta Premium", "Toque na tela para voltar");
        _qr = "https://www.spotify.com/premium/";
        setRow(0, IconID::Spotify, "Esta conta", "não é Premium", C_WARN);
        setRow(1, IconID::Cloud, "Assine em", "spotify.com/premium", C_WHITE);
        setRow(2, IconID::Settings, "Ou troque a conta", "Configurações", C_WHITE);
    } else {
        setHeader(0, "Conta não liberada", "Este app do Spotify não aceita a sua conta", "Toque na tela para voltar");
        _qr = GUIDE_URL;
        setRow(0, IconID::Settings, "No Spotify Dashboard", "User Management", C_WHITE);
        setRow(1, IconID::Phone, "Adicione a conta", "nome e e-mail", C_WHITE);
        setRow(2, IconID::Spotify, "Ou crie o seu app", "guia no QR", C_WHITE);
    }
}

void AccountView::onTap(uint16_t, uint16_t) { UIViewManager::getInstance().back(); }
