#include "msimeui/Scene.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>

namespace msimeui
{
namespace
{
constexpr float kScenePaddingDips = 5.0f;
}

void Scene::SetRoot(std::shared_ptr<Visual> root)
{
    root_ = std::move(root);
    if (root_ && window_)
    {
        root_->Attach(window_);
    }
    measureDirty_ = true;
    arrangeDirty_ = true;
}

void Scene::Attach(Window *window)
{
    window_ = window;
    if (root_)
    {
        root_->Attach(window);
    }
}

void Scene::EnsureLayout(const SizeF &size)
{
    const bool sizeChanged = !hasLastLayoutSize_ || lastLayoutSize_.width != size.width || lastLayoutSize_.height != size.height;
    if (sizeChanged || measureDirty_ || arrangeDirty_)
    {
        Layout(size);
    }
}

void Scene::Layout(const SizeF &size)
{
    if (!root_)
    {
        lastLayoutSize_ = size;
        hasLastLayoutSize_ = true;
        measureDirty_ = false;
        arrangeDirty_ = false;
        return;
    }

    const SizeF innerSize = {std::max(size.width - kScenePaddingDips * 2.0f, 0.0f),
                             std::max(size.height - kScenePaddingDips * 2.0f, 0.0f)};
    root_->MeasureInLayout(innerSize);
    root_->ArrangeInLayout({kScenePaddingDips, kScenePaddingDips, innerSize.width, innerSize.height});
    lastLayoutSize_ = size;
    hasLastLayoutSize_ = true;
    measureDirty_ = false;
    arrangeDirty_ = false;
}

void Scene::InvalidateMeasure()
{
    measureDirty_ = true;
    arrangeDirty_ = true;
    InvalidateVisual();
}

void Scene::InvalidateArrange()
{
    arrangeDirty_ = true;
    InvalidateVisual();
}

void Scene::InvalidateVisual()
{
    if (window_)
    {
        window_->Invalidate();
    }
}

void Scene::Render(DeviceResources &deviceResources)
{
    if (root_)
    {
        root_->Render(deviceResources);
    }
}

Visual *Scene::FindVisualAt(const PointF &point)
{
    return root_ ? root_->FindVisualAt(point) : nullptr;
}

Visual *Scene::FindFocusableAt(const PointF &point)
{
    return root_ ? root_->FindFocusableAt(point) : nullptr;
}

bool Scene::OnMouseWheel(const POINT &point, short delta, WPARAM keyState)
{
    return root_ ? root_->OnMouseWheel(point, delta, keyState) : false;
}
} // namespace msimeui
