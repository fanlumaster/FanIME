#include "DemoScene.h"

#include "msimeui/Controls.h"
#include "msimeui/DeviceResources.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <wrl/client.h>

namespace msimeui
{
namespace
{
class OverlayPanel : public Panel
{
  public:
    SizeF Measure(const SizeF &availableSize) override
    {
        SizeF measured = {};
        for (const auto &child : children_)
        {
            const SizeF childSize = child->Measure(availableSize);
            measured.width = std::max(measured.width, childSize.width);
            measured.height = std::max(measured.height, childSize.height);
        }
        return measured;
    }

    void Arrange(const RectF &finalRect) override
    {
        bounds_ = finalRect;
        for (const auto &child : children_)
        {
            child->Arrange(finalRect);
        }
    }

    void Render(DeviceResources &deviceResources) override
    {
        for (const auto &child : children_)
        {
            child->Render(deviceResources);
        }
    }
};

class DebugRectOverlay : public Visual
{
  public:
    explicit DebugRectOverlay(D2D1_COLOR_F color) : color_(color)
    {
    }

    SizeF Measure(const SizeF &availableSize) override
    {
        (void)availableSize;
        return {0.0f, 0.0f};
    }

    void Arrange(const RectF &finalRect) override
    {
        bounds_ = finalRect;
    }

    void Render(DeviceResources &deviceResources) override
    {
        ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
        if (!target)
        {
            return;
        }

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(color_, brush.GetAddressOf());
        const auto rect = D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height);
        target->DrawRectangle(rect, brush.Get(), 2.0f);
    }

  private:
    D2D1_COLOR_F color_;
};
} // namespace

std::unique_ptr<Scene> CreateDemoScene()
{
    auto root = std::make_shared<StackPanel>(0.0f);
    root->AddChild(std::make_shared<Spacer>(28.0f));
    root->AddChild(std::make_shared<TextBlock>(L"msimeui", 34.0f, D2D1::ColorF(0x111827), true));
    root->AddChild(std::make_shared<Spacer>(12.0f));
    root->AddChild(std::make_shared<TextBlock>(
        L"Direct2D / DirectWrite rendering with an in-tree TSF text input implementation.",
        16.0f, D2D1::ColorF(0x5B6472)));
    root->AddChild(std::make_shared<Spacer>(36.0f));

    Brush surface;
    surface.fill = D2D1::ColorF(0xFFFFFF);
    surface.stroke = D2D1::ColorF(0xD6DCE5);
    surface.strokeWidth = 1.0f;
    surface.radiusX = 22.0f;
    surface.radiusY = 22.0f;

    auto editorCard = std::make_shared<Card>(surface, 20.0f);
    auto editorStack = std::make_shared<StackPanel>(16.0f);
    editorStack->AddChild(std::make_shared<TextBlock>(L"TSF Editor", 20.0f, D2D1::ColorF(0x1F2937), true));
    editorStack->AddChild(std::make_shared<TextBlock>(
        L"Multi-line input area with TSF integration.",
        14.0f, D2D1::ColorF(0x6B7280)));
    auto editorTextBoxOverlay = std::make_shared<OverlayPanel>();
    editorTextBoxOverlay->AddChild(std::make_shared<TextBox>(220.0f, L""));
    editorTextBoxOverlay->AddChild(std::make_shared<DebugRectOverlay>(D2D1::ColorF(D2D1::ColorF::Red, 0.85f)));
    editorStack->AddChild(editorTextBoxOverlay);
    editorCard->AddChild(editorStack);
    root->AddChild(editorCard);
    root->AddChild(std::make_shared<Spacer>(28.0f));

    auto searchCard = std::make_shared<Card>(surface, 20.0f);
    auto searchStack = std::make_shared<StackPanel>(14.0f);
    searchStack->AddChild(std::make_shared<TextBlock>(L"Search Box", 18.0f, D2D1::ColorF(0x1F2937), true));
    auto searchTextBoxOverlay = std::make_shared<OverlayPanel>();
    searchTextBoxOverlay->AddChild(std::make_shared<TextBox>(52.0f, L""));
    searchTextBoxOverlay->AddChild(std::make_shared<DebugRectOverlay>(D2D1::ColorF(D2D1::ColorF::Yellow, 0.9f)));
    searchStack->AddChild(searchTextBoxOverlay);
    searchCard->AddChild(searchStack);
    root->AddChild(searchCard);

    auto scene = std::make_unique<Scene>();
    scene->SetRoot(root);
    return scene;
}
} // namespace msimeui
