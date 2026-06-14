#pragma once

#include "Layout.h"

#include <memory>

namespace msimeui
{
class Scene
{
  public:
    void SetRoot(std::shared_ptr<Visual> root);
    void Attach(Window *window);
    void EnsureLayout(const SizeF &size);
    void Layout(const SizeF &size);
    void InvalidateMeasure();
    void InvalidateMeasure(Visual *source);
    void InvalidateArrange();
    void InvalidateArrange(Visual *source);
    void InvalidateVisual();
    void Render(DeviceResources &deviceResources);
    Visual *FindVisualAt(const PointF &point);
    Visual *FindFocusableAt(const PointF &point);
    bool OnMouseWheel(const POINT &point, short delta, WPARAM keyState);

  private:
    std::shared_ptr<Visual> root_;
    Window *window_ = nullptr;
    Visual *measureDirtySource_ = nullptr;
    Visual *arrangeDirtyRoot_ = nullptr;
    SizeF lastLayoutSize_ = {};
    bool hasLastLayoutSize_ = false;
    bool measureDirty_ = true;
    bool arrangeDirty_ = true;
};
} // namespace msimeui
