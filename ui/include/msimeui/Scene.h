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
    void Layout(const SizeF &size);
    void Render(DeviceResources &deviceResources);
    Visual *FindVisualAt(const PointF &point);
    Visual *FindFocusableAt(const PointF &point);

  private:
    std::shared_ptr<Visual> root_;
};
} // namespace msimeui
