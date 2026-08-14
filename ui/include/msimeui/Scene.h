#pragma once

#include "Layout.h"

#include <functional>
#include <memory>
#include <vector>

namespace msimeui
{
class Scene
{
  public:
    struct PopupEntry
    {
        std::shared_ptr<Visual> visual;
        std::function<void()> onClose;
    };

    void SetRoot(std::shared_ptr<Visual> root);
    void Attach(Window *window);
    void EnsureLayout(const SizeF &size);
    void Layout(const SizeF &size);
    void AddPopup(std::shared_ptr<Visual> popup, std::function<void()> onClose = {});
    void RemovePopup(Visual *popup, bool notify = true);
    void ClearPopups(bool notify = true);
    bool DismissPopupsForClick(const PointF &point, Visual *target);
    void RelayoutPopups();
    void InvalidateMeasure();
    void InvalidateMeasure(Visual *source);
    void InvalidateArrange();
    void InvalidateArrange(Visual *source);
    void InvalidateVisual();
    void Render(DeviceResources &deviceResources);
    Visual *FindVisualAt(const PointF &point);
    Visual *FindFocusableAt(const PointF &point);
    bool OnMouseWheel(const POINT &point, short delta, WPARAM keyState);

    bool OnTimer(UINT_PTR timerId);

  private:
    std::shared_ptr<Visual> root_;
    std::vector<PopupEntry> popups_;
    Window *window_ = nullptr;
    Visual *measureDirtySource_ = nullptr;
    Visual *arrangeDirtyRoot_ = nullptr;
    SizeF lastLayoutSize_ = {};
    bool hasLastLayoutSize_ = false;
    bool measureDirty_ = true;
    bool arrangeDirty_ = true;
};
} // namespace msimeui
