#include "msimeui/Scene.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

namespace msimeui
{
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

    root_->Measure(size);
    root_->Arrange({0.0f, 0.0f, size.width, size.height});
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
} // namespace msimeui
