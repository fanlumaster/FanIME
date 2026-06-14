#include "msimeui/Scene.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>

namespace msimeui
{
namespace
{
constexpr float kScenePaddingDips = 5.0f;

Visual *FindCommonAncestor(Visual *lhs, Visual *rhs)
{
    if (!lhs)
    {
        return rhs;
    }
    if (!rhs)
    {
        return lhs;
    }
    if (lhs == rhs)
    {
        return lhs;
    }

    for (Visual *left = lhs; left; left = left->GetParentVisual())
    {
        for (Visual *right = rhs; right; right = right->GetParentVisual())
        {
            if (left == right)
            {
                return left;
            }
        }
    }

    return lhs;
}

bool RemeasureDirtyPath(Visual *source, Visual *root)
{
    if (!source || !root)
    {
        return false;
    }

    std::vector<Visual *> path;
    for (Visual *current = source; current; current = current->GetParentVisual())
    {
        path.push_back(current);
        if (current == root)
        {
            break;
        }
    }

    if (path.empty() || path.back() != root)
    {
        return false;
    }

    for (Visual *current : path)
    {
        if (!current->HasMeasureSlot())
        {
            return false;
        }
    }

    for (Visual *current : path)
    {
        current->MeasureInLayout(current->GetLastMeasureAvailableSize());
    }

    return true;
}
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
    measureDirtySource_ = root_.get();
    arrangeDirtyRoot_ = root_.get();
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
    const bool sizeChanged = !hasLastLayoutSize_ || lastLayoutSize_.width != size.width || lastLayoutSize_.height != size.height;
    const bool needsMeasure = sizeChanged || measureDirty_ || !hasLastLayoutSize_;
    const bool needsArrange = needsMeasure || sizeChanged || arrangeDirty_ || !hasLastLayoutSize_;

    if (!root_)
    {
        lastLayoutSize_ = size;
        hasLastLayoutSize_ = true;
        measureDirty_ = false;
        arrangeDirty_ = false;
        measureDirtySource_ = nullptr;
        arrangeDirtyRoot_ = nullptr;
        return;
    }

    const SizeF innerSize = {std::max(size.width - kScenePaddingDips * 2.0f, 0.0f),
                             std::max(size.height - kScenePaddingDips * 2.0f, 0.0f)};
    if (needsMeasure)
    {
        const bool canUseDirtyPath = !sizeChanged && measureDirtySource_ && measureDirtySource_ != root_.get();
        if (!canUseDirtyPath || !RemeasureDirtyPath(measureDirtySource_, root_.get()))
        {
            root_->MeasureInLayout(innerSize);
        }
    }
    if (needsArrange)
    {
        const RectF rootSlot = {kScenePaddingDips, kScenePaddingDips, innerSize.width, innerSize.height};
        if (!needsMeasure && !sizeChanged && arrangeDirtyRoot_ && arrangeDirtyRoot_ != root_.get() &&
            arrangeDirtyRoot_->HasArrangeSlot())
        {
            arrangeDirtyRoot_->ArrangeInLayout(arrangeDirtyRoot_->lastArrangeRect_);
        }
        else
        {
            root_->ArrangeInLayout(rootSlot);
        }
    }
    lastLayoutSize_ = size;
    hasLastLayoutSize_ = true;
    measureDirty_ = false;
    arrangeDirty_ = false;
    measureDirtySource_ = nullptr;
    arrangeDirtyRoot_ = nullptr;
}

void Scene::InvalidateMeasure()
{
    measureDirty_ = true;
    arrangeDirty_ = true;
    measureDirtySource_ = root_.get();
    arrangeDirtyRoot_ = root_.get();
    InvalidateVisual();
}

void Scene::InvalidateMeasure(Visual *source)
{
    measureDirty_ = true;
    arrangeDirty_ = true;
    if (!source || !root_)
    {
        measureDirtySource_ = root_.get();
        arrangeDirtyRoot_ = root_.get();
    }
    else if (!measureDirtySource_)
    {
        measureDirtySource_ = source;
        arrangeDirtyRoot_ = source;
    }
    else
    {
        measureDirtySource_ = FindCommonAncestor(measureDirtySource_, source);
        arrangeDirtyRoot_ = FindCommonAncestor(arrangeDirtyRoot_, source);
    }
    InvalidateVisual();
}

void Scene::InvalidateArrange()
{
    arrangeDirty_ = true;
    arrangeDirtyRoot_ = root_.get();
    InvalidateVisual();
}

void Scene::InvalidateArrange(Visual *source)
{
    arrangeDirty_ = true;
    if (!source || !root_)
    {
        arrangeDirtyRoot_ = root_.get();
    }
    else if (!arrangeDirtyRoot_)
    {
        arrangeDirtyRoot_ = source;
    }
    else
    {
        arrangeDirtyRoot_ = FindCommonAncestor(arrangeDirtyRoot_, source);
    }
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
