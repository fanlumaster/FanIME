#pragma once

#include <memory>
#include <windows.h>

class CandidatePresenter
{
  public:
    static CandidatePresenter &Instance();

    bool Bind(HWND hwnd);
    bool IsBound() const;
    void ShowFromGlobalState();
    void ShowFromGlobalState(POINT caret);
    void Hide();
    void Present();
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

  private:
    CandidatePresenter();
    ~CandidatePresenter();
    CandidatePresenter(const CandidatePresenter &) = delete;
    CandidatePresenter &operator=(const CandidatePresenter &) = delete;

    void RebuildScene();
    void ApplySkin();
    void FillItemsFromUi();
    void PlaceAndShow(POINT caret, float widthDip, float heightDip, float cardLeftDip, float cardTopDip);
    void ArmHoverIfPointerMoved();
    void CommitItem(size_t pageIndex);
    void ShowItemContextMenu(size_t pageIndex, POINT clientPoint);
    void CloseContextMenu(bool restoreHost);
    void OpenFixSubmenu();
    void CloseFixSubmenu();
    void ExpandHostForMenu(POINT clientPoint);
    void RestoreHostAfterMenu();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    HWND hwnd_ = nullptr;
    bool bound_ = false;
    bool hoverArmed_ = false;
    bool ignoreSelectionCallback_ = false;
    POINT hoverBaseline_{};
    float decorationTopDip_ = 0.0f;
    float decorationWidthDip_ = 0.0f;
};
