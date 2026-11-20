/*-------------------------------------------------------------------------------------------------
** UIViewManager.h - View navigation stack manager + transient toasts
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <vector>
#include "UIView.h"

class UIViewManager {
public:
    static UIViewManager& getInstance();

    void setDisplayUI(DisplayUI *ui);

    void registerView(ViewID id, UIView *view);
    void gotoView(ViewID id);
    void back();
    void home();                        // drop the stack and show Home

    // Small pill at the bottom of the screen; the view repaints after ~2.5 s.
    void showToast(const char* msg);
    void tick();                        // call every UI frame

    UIView* getActiveView();

private:
    UIViewManager() {}
    void activate(UIView* view);

    DisplayUI *_ui = nullptr;
    std::vector<UIView*> _views;
    std::vector<ViewID> _stack;
    UIView *_activeView = nullptr;
    unsigned long _toastUntil = 0;
};
