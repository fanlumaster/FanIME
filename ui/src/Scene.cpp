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
}

void Scene::Attach(Window *window)
{
    if (root_)
    {
        root_->Attach(window);
    }
}

void Scene::Layout(const SizeF &size)
{
    if (!root_)
    {
        return;
    }

    const SizeF innerSize = {std::max(size.width - kScenePaddingDips * 2.0f, 0.0f),
                             std::max(size.height - kScenePaddingDips * 2.0f, 0.0f)};
    root_->Measure(innerSize);
    root_->Arrange({kScenePaddingDips, kScenePaddingDips, innerSize.width, innerSize.height});
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
