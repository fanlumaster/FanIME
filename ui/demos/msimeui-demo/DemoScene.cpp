#include "DemoScene.h"

#include "msimeui/Controls.h"

#include <sstream>

namespace msimeui
{
std::unique_ptr<Scene> CreateDemoScene()
{
    auto root = std::make_shared<StackPanel>(0.0f);
    root->AddChild(std::make_shared<Spacer>(28.0f));
    root->AddChild(std::make_shared<TextBlock>(L"msimeui", 34.0f, D2D1::ColorF(0x111827), true));
    root->AddChild(std::make_shared<Spacer>(12.0f));
    root->AddChild(
        std::make_shared<TextBlock>(L"A retained-mode Win32 GUI framework with Direct2D / DirectWrite and in-tree TSF.",
                                    16.0f, D2D1::ColorF(0x5B6472)));
    root->AddChild(std::make_shared<Spacer>(36.0f));

    Brush surface;
    surface.fill = D2D1::ColorF(0xFFFFFF);
    surface.stroke = D2D1::ColorF(0xD6DCE5);
    surface.strokeWidth = 1.0f;
    surface.radiusX = 22.0f;
    surface.radiusY = 22.0f;

    auto heroCard = std::make_shared<Card>(surface, 20.0f);
    auto heroStack = std::make_shared<StackPanel>(16.0f);
    auto statusText =
        std::make_shared<TextBlock>(L"Framework status: base controls are now interactive and composable.", 15.0f,
                                    D2D1::ColorF(0x0F172A));
    auto featureWrap = std::make_shared<WrapPanel>(10.0f, 10.0f);
    featureWrap->AddChild(std::make_shared<Button>(L"Direct2D"));
    featureWrap->AddChild(std::make_shared<Button>(L"TSF Input"));
    featureWrap->AddChild(std::make_shared<Button>(L"Layout"));
    featureWrap->AddChild(std::make_shared<Button>(L"Buttons"));
    featureWrap->AddChild(std::make_shared<Button>(L"Slider"));
    featureWrap->AddChild(std::make_shared<Button>(L"Progress"));

    auto actionRow = std::make_shared<HorizontalStackPanel>(12.0f);
    auto primaryButton = std::make_shared<Button>(L"Run Interaction Demo", 46.0f);
    auto secondaryButton = std::make_shared<Button>(L"Reset Status", 46.0f);
    primaryButton->SetOnClick([statusText]() {
        statusText->SetText(L"Interaction check: buttons are dispatching clicks through the visual tree.");
    });
    secondaryButton->SetOnClick([statusText]() {
        statusText->SetText(L"Framework status: base controls are now interactive and composable.");
    });
    actionRow->AddChild(primaryButton);
    actionRow->AddChild(secondaryButton);

    heroStack->AddChild(std::make_shared<TextBlock>(L"Component Gallery", 22.0f, D2D1::ColorF(0x1F2937), true));
    heroStack->AddChild(std::make_shared<TextBlock>(
        L"Common controls and containers inspired by classic GUI frameworks, but implemented in this project's current style.",
        14.0f, D2D1::ColorF(0x6B7280)));
    heroStack->AddChild(featureWrap);
    heroStack->AddChild(statusText);
    heroStack->AddChild(actionRow);
    heroCard->AddChild(heroStack);
    root->AddChild(heroCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto controlsCard = std::make_shared<Card>(surface, 20.0f);
    auto controlsStack = std::make_shared<StackPanel>(16.0f);
    auto progressLabel =
        std::make_shared<TextBlock>(L"Progress: 35%", 14.0f, D2D1::ColorF(0x475569), true);
    auto progressBar = std::make_shared<ProgressBar>(14.0f);
    progressBar->SetValue(0.35f);
    auto slider = std::make_shared<Slider>(0.0f, 100.0f, 35.0f, 34.0f);
    slider->SetOnChanged([progressBar, progressLabel](float value) {
        progressBar->SetValue(value / 100.0f);
        std::wstringstream stream;
        stream << L"Progress: " << static_cast<int>(value + 0.5f) << L"%";
        progressLabel->SetText(stream.str());
    });

    auto optionText = std::make_shared<TextBlock>(L"Selection: compact mode is enabled.", 14.0f, D2D1::ColorF(0x475569));
    auto checkboxRow = std::make_shared<HorizontalStackPanel>(18.0f);
    auto compactMode = std::make_shared<CheckBox>(L"Compact layout", true);
    auto diagnostics = std::make_shared<CheckBox>(L"Enable diagnostics", false);
    auto updateOptionText = std::make_shared<std::function<void()>>();
    *updateOptionText = [optionText, compactMode, diagnostics]() {
        if (diagnostics->IsChecked())
        {
            optionText->SetText(compactMode->IsChecked() ? L"Selection: compact mode + diagnostics are enabled."
                                                         : L"Selection: diagnostics are enabled.");
            return;
        }

        optionText->SetText(compactMode->IsChecked() ? L"Selection: compact mode is enabled."
                                                     : L"Selection: compact mode is disabled.");
    };
    compactMode->SetOnChanged([updateOptionText](bool checked) {
        (void)checked;
        (*updateOptionText)();
    });
    diagnostics->SetOnChanged([updateOptionText](bool checked) {
        (void)checked;
        (*updateOptionText)();
    });
    checkboxRow->AddChild(compactMode);
    checkboxRow->AddChild(diagnostics);

    controlsStack->AddChild(std::make_shared<TextBlock>(L"Interactive Controls", 20.0f, D2D1::ColorF(0x1F2937), true));
    controlsStack->AddChild(std::make_shared<TextBlock>(
        L"Slider, progress, buttons and toggles can now be composed into higher-level widgets and panels.",
        14.0f, D2D1::ColorF(0x6B7280)));
    controlsStack->AddChild(progressLabel);
    controlsStack->AddChild(progressBar);
    controlsStack->AddChild(slider);
    controlsStack->AddChild(std::make_shared<Separator>(1.0f));
    controlsStack->AddChild(checkboxRow);
    controlsStack->AddChild(optionText);
    controlsCard->AddChild(controlsStack);
    root->AddChild(controlsCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto editorCard = std::make_shared<Card>(surface, 20.0f);
    auto editorStack = std::make_shared<StackPanel>(16.0f);
    editorStack->AddChild(std::make_shared<TextBlock>(L"TSF Editor", 20.0f, D2D1::ColorF(0x1F2937), true));
    editorStack->AddChild(std::make_shared<TextBlock>(
        L"Multi-line input area with TSF integration. This remains the project's flagship control.",
        14.0f, D2D1::ColorF(0x6B7280)));
    editorStack->AddChild(std::make_shared<TextBox>(220.0f, L"Type with any IME here..."));
    editorCard->AddChild(editorStack);
    root->AddChild(editorCard);
    root->AddChild(std::make_shared<Spacer>(28.0f));

    auto searchCard = std::make_shared<Card>(surface, 20.0f);
    auto searchStack = std::make_shared<StackPanel>(14.0f);
    searchStack->AddChild(std::make_shared<TextBlock>(L"Search Box", 18.0f, D2D1::ColorF(0x1F2937), true));
    searchStack->AddChild(std::make_shared<TextBlock>(
        L"Single-line input can now sit alongside the broader control set as a first-class widget.",
        14.0f, D2D1::ColorF(0x6B7280)));
    searchStack->AddChild(std::make_shared<TextBox>(52.0f, L"Search controls, themes, layout..."));
    searchCard->AddChild(searchStack);
    root->AddChild(searchCard);

    auto scene = std::make_unique<Scene>();
    scene->SetRoot(std::make_shared<ScrollViewer>(root));
    return scene;
}
} // namespace msimeui
