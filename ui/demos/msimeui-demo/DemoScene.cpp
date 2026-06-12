#include "DemoScene.h"

#include "msimeui/Controls.h"

namespace msimeui
{
std::unique_ptr<Scene> CreateDemoScene()
{
    auto root = std::make_shared<StackPanel>(18.0f);
    root->AddChild(std::make_shared<TextBlock>(L"msimeui", 34.0f, D2D1::ColorF(0x111827), true));
    root->AddChild(std::make_shared<TextBlock>(
        L"A fresh Direct2D/DirectWrite GUI foundation with a TSF-capable text box hosted inside the visual tree.",
        16.0f, D2D1::ColorF(0x5B6472)));

    Brush surface;
    surface.fill = D2D1::ColorF(0xFFFFFF);
    surface.stroke = D2D1::ColorF(0xD6DCE5);
    surface.strokeWidth = 1.0f;
    surface.radiusX = 22.0f;
    surface.radiusY = 22.0f;

    auto editorCard = std::make_shared<Card>(surface, 20.0f);
    auto editorStack = std::make_shared<StackPanel>(12.0f);
    editorStack->AddChild(std::make_shared<TextBlock>(L"TSF Editor", 20.0f, D2D1::ColorF(0x1F2937), true));
    editorStack->AddChild(std::make_shared<TextBlock>(
        L"This control uses the in-tree TSF text input implementation and keeps Direct2D/DirectWrite rendering for text input.",
        14.0f, D2D1::ColorF(0x6B7280)));
    editorStack->AddChild(std::make_shared<HostedTextBox>(220.0f, L""));
    editorCard->AddChild(editorStack);
    root->AddChild(editorCard);

    auto searchCard = std::make_shared<Card>(surface, 20.0f);
    auto searchStack = std::make_shared<StackPanel>(10.0f);
    searchStack->AddChild(std::make_shared<TextBlock>(L"Search Box", 18.0f, D2D1::ColorF(0x1F2937), true));
    searchStack->AddChild(std::make_shared<HostedTextBox>(52.0f, L""));
    searchCard->AddChild(searchStack);
    root->AddChild(searchCard);

    auto scene = std::make_unique<Scene>();
    scene->SetRoot(root);
    return scene;
}
} // namespace msimeui
