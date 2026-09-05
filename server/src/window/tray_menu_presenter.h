#pragma once

#include <memory>
#include <windows.h>

class TrayMenuPresenter
{
  public:
    static TrayMenuPresenter &Instance();

    bool Bind(HWND hwnd);
    bool IsBound() const;
    bool IsOpenToUser() const;
    void ShowFromLangBar();
    void Hide();
    void Present();
    void SyncFloatingToolbarToggle();
    void ApplyTheme();
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

  private:
    TrayMenuPresenter();
    ~TrayMenuPresenter();
    TrayMenuPresenter(const TrayMenuPresenter &) = delete;
    TrayMenuPresenter &operator=(const TrayMenuPresenter &) = delete;

    void RebuildScene();
    void PlaceAndShow(float widthDip, float heightDip);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    HWND hwnd_ = nullptr;
    bool bound_ = false;
    bool openToUser_ = false;
};
