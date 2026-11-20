/*-------------------------------------------------------------------------------------------------
** UIViewManager.cpp - View navigation implementation
** ------------------------------------------------------------------------------------------------
*/
#include "UIViewManager.h"

UIViewManager& UIViewManager::getInstance() {
    static UIViewManager instance;
    return instance;
}

void UIViewManager::setDisplayUI(DisplayUI *ui) {
    _ui = ui;
    _views.resize(8);
}

void UIViewManager::registerView(ViewID id, UIView *view) {
    int idx = static_cast<int>(id);
    if (idx < (int)_views.size()) _views[idx] = view;
}

void UIViewManager::activate(UIView* view) {
    _toastUntil = 0;
    _activeView = view;
    _activeView->onEnter();
    _activeView->markDirty();
    _activeView->draw();
    _activeView->markClean();
}

void UIViewManager::gotoView(ViewID id) {
    int idx = static_cast<int>(id);
    if (idx >= (int)_views.size() || _views[idx] == nullptr) return;
    if (_activeView) {
        if (_activeView == _views[idx]) return;
        _activeView->onExit();
        // Boot is never a back target.
        if (_activeView->getID() != ViewID::Boot) _stack.push_back(_activeView->getID());
    }
    activate(_views[idx]);
}

void UIViewManager::back() {
    if (_stack.empty()) { home(); return; }
    if (_activeView) _activeView->onExit();
    ViewID prev = _stack.back();
    _stack.pop_back();
    activate(_views[static_cast<int>(prev)]);
}

void UIViewManager::home() {
    _stack.clear();
    UIView* homeView = _views[static_cast<int>(ViewID::Home)];
    if (!homeView || _activeView == homeView) return;
    if (_activeView) _activeView->onExit();
    activate(homeView);
}

void UIViewManager::showToast(const char* msg) {
    if (!_ui || !msg || !*msg) return;
    int32_t w = min((int32_t)(SCREEN_W - 24), _ui->textWidth(msg, FONT_SM) + 44);
    int32_t x = (SCREEN_W - w) / 2, y = SCREEN_H - 46, h = 32;
    _ui->fillRoundRect(x - 2, y - 2, w + 4, h + 4, h / 2 + 2, C_BG, C_BG);   // clean edge over any content
    _ui->fillRoundRect(x, y, w, h, h / 2, C_BUTTON, C_BG);
    _ui->drawIcon(IconID::Alert, x + 18, y + h / 2, 14, C_WARN, C_BUTTON);
    _ui->setTextColor(C_WHITE, C_BUTTON);
    _ui->drawString(_ui->fitText(msg, w - 44, FONT_SM).c_str(), x + 32, _ui->textY(FONT_SM, y + h / 2), FONT_SM);
    _toastUntil = millis() + 2500;
}

void UIViewManager::tick() {
    if (_toastUntil && (long)(millis() - _toastUntil) > 0) {
        _toastUntil = 0;
        if (_activeView) _activeView->markDirty();
    }
}

UIView* UIViewManager::getActiveView() {
    return _activeView;
}
