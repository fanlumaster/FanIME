#include "DemoScene.h"

#include "msimeui/Controls.h"

#include <sstream>

namespace msimeui
{
std::unique_ptr<Scene> CreateDemoScene()
{
    auto root = std::make_shared<StackPanel>(0.0f);
    root->AddChild(std::make_shared<Spacer>(14.0f));
    auto appHeader = std::make_shared<StackPanel>(12.0f);
    auto appTitle = std::make_shared<TextBlock>(L"msimeui", 34.0f, D2D1::ColorF(0x111827), true);
    appTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    auto appSubtitle =
        std::make_shared<TextBlock>(L"A retained-mode Win32 GUI framework with Direct2D / DirectWrite and in-tree TSF.",
                                    16.0f, D2D1::ColorF(0x5B6472));
    appSubtitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    appHeader->AddChild(appTitle);
    appHeader->AddChild(appSubtitle);
    root->AddChild(appHeader);
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
    featureWrap->SetHorizontalAlignment(HorizontalAlignment::Leading);
    auto direct2dChip = std::make_shared<Button>(L"Direct2D");
    direct2dChip->SetWidth(120.0f);
    auto tsfChip = std::make_shared<Button>(L"TSF Input");
    tsfChip->SetWidth(120.0f);
    auto layoutChip = std::make_shared<Button>(L"Layout");
    layoutChip->SetWidth(108.0f);
    auto buttonChip = std::make_shared<Button>(L"Buttons");
    buttonChip->SetWidth(112.0f);
    auto sliderChip = std::make_shared<Button>(L"Slider");
    sliderChip->SetWidth(104.0f);
    auto progressChip = std::make_shared<Button>(L"Progress");
    progressChip->SetWidth(114.0f);
    featureWrap->AddChild(direct2dChip);
    featureWrap->AddChild(tsfChip);
    featureWrap->AddChild(layoutChip);
    featureWrap->AddChild(buttonChip);
    featureWrap->AddChild(sliderChip);
    featureWrap->AddChild(progressChip);

    auto actionRow = std::make_shared<HorizontalStackPanel>(12.0f);
    auto primaryButton = std::make_shared<Button>(L"Run Interaction Demo", 46.0f);
    auto secondaryButton = std::make_shared<Button>(L"Reset Status", 46.0f);
    primaryButton->SetWidth(220.0f);
    secondaryButton->SetWidth(160.0f);
    secondaryButton->SetMinWidth(140.0f);
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

    Brush insetBrush;
    insetBrush.fill = D2D1::ColorF(0xF8FAFC);
    insetBrush.stroke = D2D1::ColorF(0xCBD5E1);
    insetBrush.strokeWidth = 1.0f;
    insetBrush.radiusX = 16.0f;
    insetBrush.radiusY = 16.0f;

    auto insetText = std::make_shared<TextBlock>(
        L"Border is a general-purpose single-child container with fill, stroke and padding. It is lighter-weight than Card for simple callouts.",
        13.0f, D2D1::ColorF(0x475569));
    auto insetBorder = std::make_shared<Border>(insetBrush, insetText);
    insetBorder->SetPadding({14.0f, 12.0f, 14.0f, 12.0f});
    heroStack->AddChild(insetBorder);
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
    optionText->SetMaxWidth(520.0f);
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

    auto alignmentNote = std::make_shared<TextBlock>(
        L"These controls now use shared layout properties like explicit width and per-control margin.",
        14.0f, D2D1::ColorF(0x64748B));
    alignmentNote->SetMargin({0.0f, 6.0f, 0.0f, 0.0f});
    alignmentNote->SetMaxWidth(560.0f);
    controlsStack->AddChild(alignmentNote);

    auto constraintRow = std::make_shared<HorizontalStackPanel>(12.0f);
    constraintRow->SetMargin({0.0f, 10.0f, 0.0f, 0.0f});
    auto minButton = std::make_shared<Button>(L"Min 150", 40.0f);
    minButton->SetMinWidth(150.0f);
    minButton->SetHorizontalAlignment(HorizontalAlignment::Leading);
    auto boundedButton = std::make_shared<Button>(L"80-140", 40.0f);
    boundedButton->SetMinWidth(80.0f);
    boundedButton->SetMaxWidth(140.0f);
    boundedButton->SetWidth(120.0f);
    boundedButton->SetHorizontalAlignment(HorizontalAlignment::Leading);
    constraintRow->AddChild(minButton);
    constraintRow->AddChild(boundedButton);
    controlsStack->AddChild(constraintRow);
    controlsCard->AddChild(controlsStack);
    root->AddChild(controlsCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto alignmentCard = std::make_shared<Card>(surface, 20.0f);
    auto alignmentStack = std::make_shared<StackPanel>(14.0f);
    auto centeredRow = std::make_shared<HorizontalStackPanel>(10.0f);
    centeredRow->SetHorizontalContentAlignment(HorizontalAlignment::Center);
    centeredRow->SetVerticalContentAlignment(VerticalAlignment::Center);
    centeredRow->SetHeight(70.0f);
    centeredRow->AddChild(std::make_shared<Button>(L"Centered", 38.0f));
    centeredRow->AddChild(std::make_shared<Button>(L"Actions", 38.0f));

    auto trailingColumn = std::make_shared<StackPanel>(8.0f);
    trailingColumn->SetHorizontalContentAlignment(HorizontalAlignment::Trailing);
    trailingColumn->SetWidth(320.0f);
    auto smallA = std::make_shared<Button>(L"Trailing A", 36.0f);
    smallA->SetWidth(120.0f);
    auto smallB = std::make_shared<Button>(L"Trailing B", 36.0f);
    smallB->SetWidth(150.0f);
    trailingColumn->AddChild(smallA);
    trailingColumn->AddChild(smallB);

    alignmentStack->AddChild(std::make_shared<TextBlock>(L"Container Alignment", 20.0f, D2D1::ColorF(0x1F2937), true));
    alignmentStack->AddChild(std::make_shared<TextBlock>(
        L"Panels can now center or trailing-align whole runs of children, instead of only stretching from the leading edge.",
        14.0f, D2D1::ColorF(0x6B7280)));
    alignmentStack->AddChild(centeredRow);
    alignmentStack->AddChild(trailingColumn);

    auto paddingNote = std::make_shared<TextBlock>(
        L"Container and Border also support shared padding, so content spacing no longer needs to be hardcoded into every specialized control.",
        14.0f, D2D1::ColorF(0x475569));
    auto paddingContainer = std::make_shared<Container>(paddingNote);
    paddingContainer->SetPadding({18.0f, 14.0f, 18.0f, 14.0f});
    paddingContainer->SetMargin({0.0f, 8.0f, 0.0f, 0.0f});
    alignmentStack->AddChild(paddingContainer);
    alignmentCard->AddChild(alignmentStack);
    root->AddChild(alignmentCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto gridCard = std::make_shared<Card>(surface, 20.0f);
    auto gridStack = std::make_shared<StackPanel>(14.0f);
    auto formGrid = std::make_shared<Grid>();
    formGrid->AddColumn({GridUnitType::Pixel, 150.0f});
    formGrid->AddColumn({GridUnitType::Star, 1.0f});
    formGrid->AddRow({GridUnitType::Auto, 0.0f});
    formGrid->AddRow({GridUnitType::Auto, 0.0f});
    formGrid->AddRow({GridUnitType::Auto, 0.0f});
    formGrid->SetColumnSpacing(14.0f);
    formGrid->SetRowSpacing(12.0f);

    auto nameLabel = std::make_shared<TextBlock>(L"Project Name", 14.0f, D2D1::ColorF(0x334155), true);
    nameLabel->SetVerticalAlignment(VerticalAlignment::Center);
    auto modeLabel = std::make_shared<TextBlock>(L"Input Mode", 14.0f, D2D1::ColorF(0x334155), true);
    modeLabel->SetVerticalAlignment(VerticalAlignment::Center);
    auto noteLabel = std::make_shared<TextBlock>(L"Notes", 14.0f, D2D1::ColorF(0x334155), true);
    noteLabel->SetVerticalAlignment(VerticalAlignment::Leading);

    auto nameBox = std::make_shared<TextBox>(52.0f, L"msimeui demo");
    auto modeRow = std::make_shared<HorizontalStackPanel>(10.0f);
    modeRow->SetVerticalContentAlignment(VerticalAlignment::Center);
    auto modeCore = std::make_shared<CheckBox>(L"Core IME mode", true);
    auto modeExt = std::make_shared<CheckBox>(L"Extended diagnostics", false);
    modeRow->AddChild(modeCore);
    modeRow->AddChild(modeExt);
    auto noteBorder = std::make_shared<Border>(insetBrush, std::make_shared<TextBlock>(
                                                               L"Grid is now suitable for forms, property panes, and two-column settings screens.",
                                                               14.0f, D2D1::ColorF(0x475569)));
    noteBorder->SetPadding({14.0f, 12.0f, 14.0f, 12.0f});

    formGrid->AddChild(nameLabel, 0, 0);
    formGrid->AddChild(nameBox, 0, 1);
    formGrid->AddChild(modeLabel, 1, 0);
    formGrid->AddChild(modeRow, 1, 1);
    formGrid->AddChild(noteLabel, 2, 0);
    formGrid->AddChild(noteBorder, 2, 1);

    gridStack->AddChild(std::make_shared<TextBlock>(L"Grid Layout", 20.0f, D2D1::ColorF(0x1F2937), true));
    gridStack->AddChild(std::make_shared<TextBlock>(
        L"A simple Auto / Pixel / Star grid is now available for forms, settings panes, and split content layouts.",
        14.0f, D2D1::ColorF(0x6B7280)));
    gridStack->AddChild(formGrid);
    gridCard->AddChild(gridStack);
    root->AddChild(gridCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto listCard = std::make_shared<Card>(surface, 20.0f);
    auto listStack = std::make_shared<StackPanel>(14.0f);
    auto listStatus = std::make_shared<TextBlock>(L"Selected module: Core Input", 14.0f, D2D1::ColorF(0x475569), true);
    auto listView = std::make_shared<ListView>(72.0f);
    listView->AddItem({L"Core Input", L"TSF composition, caret handling, and editor bridge", L"Active"});
    listView->AddItem({L"Layout System", L"Shared sizing, alignment, grid, and scrolling primitives", L"Stable"});
    listView->AddItem({L"Control Pack", L"Buttons, toggles, sliders, and list interactions", L"New"});
    listView->SetOnSelectionChanged([listStatus, listView](size_t selectedIndex) {
        static const wchar_t *names[] = {L"Core Input", L"Layout System", L"Control Pack"};
        if (selectedIndex < 3)
        {
            std::wstring text = L"Selected module: ";
            text += names[selectedIndex];
            listStatus->SetText(text);
        }
        (void)listView;
    });

    listStack->AddChild(std::make_shared<TextBlock>(L"ListView", 20.0f, D2D1::ColorF(0x1F2937), true));
    listStack->AddChild(std::make_shared<TextBlock>(
        L"A first-pass selectable list control is now available for navigation panes, settings groups, and data summaries.",
        14.0f, D2D1::ColorF(0x6B7280)));
    listStack->AddChild(listStatus);
    listStack->AddChild(listView);
    listCard->AddChild(listStack);
    root->AddChild(listCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto treeCard = std::make_shared<Card>(surface, 20.0f);
    auto treeStack = std::make_shared<StackPanel>(14.0f);
    auto treeStatus = std::make_shared<TextBlock>(L"Selected node: msimeui", 14.0f, D2D1::ColorF(0x475569), true);
    auto treeView = std::make_shared<TreeView>(64.0f);

    TreeView::Node rootNode;
    rootNode.title = L"msimeui";
    rootNode.subtitle = L"Framework root";
    rootNode.expanded = true;
    rootNode.children = {
        {L"Layout", L"Panels, grid, alignment, and sizing", true, {}},
        {L"Controls", L"Buttons, inputs, lists, and tree interactions", true,
         {
             {L"TextBox", L"TSF-backed text editor control", true, {}},
             {L"ListView", L"Selectable summary list", true, {}},
             {L"TreeView", L"Expandable hierarchy view", true, {}},
         }},
        {L"Rendering", L"Direct2D and DirectWrite device resources", true, {}},
    };
    treeView->AddRoot(std::move(rootNode));
    treeView->SetOnSelectionChanged([treeStatus](const std::wstring &selectedTitle) {
        treeStatus->SetText(L"Selected node: " + selectedTitle);
    });

    treeStack->AddChild(std::make_shared<TextBlock>(L"TreeView", 20.0f, D2D1::ColorF(0x1F2937), true));
    treeStack->AddChild(std::make_shared<TextBlock>(
        L"A first-pass hierarchy control is now available for navigation trees, grouped settings, and project/module explorers.",
        14.0f, D2D1::ColorF(0x6B7280)));
    treeStack->AddChild(treeStatus);
    treeStack->AddChild(treeView);
    treeCard->AddChild(treeStack);
    root->AddChild(treeCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto tabsCard = std::make_shared<Card>(surface, 20.0f);
    auto tabsStack = std::make_shared<StackPanel>(14.0f);
    auto tabStatus = std::make_shared<TextBlock>(L"Active tab: Overview", 14.0f, D2D1::ColorF(0x475569), true);
    auto tabControl = std::make_shared<TabControl>(46.0f);

    auto overviewPanel = std::make_shared<StackPanel>(10.0f);
    auto overviewText = std::make_shared<TextBlock>(
        L"This tab groups the framework highlights into a compact summary page.",
        14.0f, D2D1::ColorF(0x475569));
    overviewText->SetMargin({0.0f, 0.0f, 0.0f, 4.0f});
    auto overviewAction = std::make_shared<Button>(L"Overview Action", 40.0f);
    overviewAction->SetWidth(220.0f);
    overviewAction->SetHorizontalAlignment(HorizontalAlignment::Leading);
    overviewPanel->AddChild(overviewText);
    overviewPanel->AddChild(overviewAction);
    auto overviewContent = std::make_shared<Container>(overviewPanel);
    overviewContent->SetPadding({16.0f, 16.0f, 16.0f, 16.0f});

    auto layoutPanel = std::make_shared<StackPanel>(10.0f);
    layoutPanel->AddChild(std::make_shared<TextBlock>(
        L"Layout includes stack panels, wrap, scroll, grid, shared sizing, and alignment.",
        14.0f, D2D1::ColorF(0x475569)));
    auto layoutBadgeRow = std::make_shared<HorizontalStackPanel>(10.0f);
    auto gridBadge = std::make_shared<Button>(L"Grid", 38.0f);
    gridBadge->SetWidth(90.0f);
    auto scrollBadge = std::make_shared<Button>(L"Scroll", 38.0f);
    scrollBadge->SetWidth(90.0f);
    layoutBadgeRow->AddChild(gridBadge);
    layoutBadgeRow->AddChild(scrollBadge);
    layoutPanel->AddChild(layoutBadgeRow);
    auto layoutContent = std::make_shared<Container>(layoutPanel);
    layoutContent->SetPadding({16.0f, 16.0f, 16.0f, 16.0f});

    auto navigationPanel = std::make_shared<StackPanel>(10.0f);
    navigationPanel->AddChild(std::make_shared<TextBlock>(
        L"Navigation now includes ListView, TreeView, and this TabControl container.",
        14.0f, D2D1::ColorF(0x475569)));
    auto navBorder = std::make_shared<Border>(insetBrush, std::make_shared<TextBlock>(
                                                              L"Container-style pages can be swapped without rebuilding the visual tree.",
                                                              13.0f, D2D1::ColorF(0x475569)));
    navBorder->SetPadding({14.0f, 12.0f, 14.0f, 12.0f});
    navigationPanel->AddChild(navBorder);
    auto navigationContent = std::make_shared<Container>(navigationPanel);
    navigationContent->SetPadding({16.0f, 16.0f, 16.0f, 16.0f});

    tabControl->AddTab(L"Overview", overviewContent);
    tabControl->AddTab(L"Layout", layoutContent);
    tabControl->AddTab(L"Navigation", navigationContent);
    tabControl->SetHeight(220.0f);
    tabControl->SetOnSelectionChanged([tabStatus](size_t selectedIndex) {
        static const wchar_t *names[] = {L"Overview", L"Layout", L"Navigation"};
        if (selectedIndex < 3)
        {
            tabStatus->SetText(std::wstring(L"Active tab: ") + names[selectedIndex]);
        }
    });

    tabsStack->AddChild(std::make_shared<TextBlock>(L"TabControl", 20.0f, D2D1::ColorF(0x1F2937), true));
    tabsStack->AddChild(std::make_shared<TextBlock>(
        L"A first-pass tab container is now available for settings pages, tool panes, and grouped work areas.",
        14.0f, D2D1::ColorF(0x6B7280)));
    tabsStack->AddChild(tabStatus);
    tabsStack->AddChild(tabControl);
    tabsCard->AddChild(tabsStack);
    root->AddChild(tabsCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto popupCard = std::make_shared<Card>(surface, 20.0f);
    auto popupStack = std::make_shared<StackPanel>(14.0f);
    auto popupStatus = std::make_shared<TextBlock>(L"Popup status: closed", 14.0f, D2D1::ColorF(0x475569), true);
    auto popupTrigger = std::make_shared<Button>(L"Open Quick Actions", 44.0f);
    popupTrigger->SetWidth(200.0f);

    auto popupContent = std::make_shared<StackPanel>(12.0f);
    popupContent->SetHorizontalContentAlignment(HorizontalAlignment::Leading);
    popupContent->AddChild(std::make_shared<TextBlock>(L"Quick Actions", 16.0f, D2D1::ColorF(0x1F2937), true));
    auto popupDescription = std::make_shared<TextBlock>(
        L"This popup is rendered as a scene-level overlay. It sits above the normal visual tree and closes on outside click.",
        13.0f, D2D1::ColorF(0x64748B));
    popupDescription->SetMaxWidth(240.0f);
    popupDescription->SetMargin({0.0f, 2.0f, 0.0f, 6.0f});
    popupContent->AddChild(popupDescription);
    auto popupActionA = std::make_shared<Button>(L"Mark Stable", 38.0f);
    auto popupActionB = std::make_shared<Button>(L"Mark Experimental", 38.0f);
    popupActionA->SetWidth(160.0f);
    popupActionB->SetWidth(190.0f);
    popupActionA->SetMargin({0.0f, 16.0f, 0.0f, 0.0f});
    popupActionA->SetHorizontalAlignment(HorizontalAlignment::Leading);
    popupActionB->SetHorizontalAlignment(HorizontalAlignment::Leading);
    popupActionA->SetOnClick([popupStatus]() { popupStatus->SetText(L"Popup status: marked stable"); });
    popupActionB->SetOnClick([popupStatus]() { popupStatus->SetText(L"Popup status: marked experimental"); });
    popupContent->AddChild(popupActionA);
    popupContent->AddChild(popupActionB);

    auto popup = std::make_shared<Popup>(popupContent);
    popup->SetMatchAnchorWidth(false);
    popup->SetWidth(300.0f);
    popup->SetPadding({18.0f, 18.0f, 18.0f, 18.0f});

    auto popupHost = std::make_shared<PopupHost>(popupTrigger, popup);

    popupStack->AddChild(std::make_shared<TextBlock>(L"PopupHost / Popup", 20.0f, D2D1::ColorF(0x1F2937), true));
    popupStack->AddChild(std::make_shared<TextBlock>(
        L"A first overlay path is now available for menus, quick actions, tooltips, and future combo-box style interactions.",
        14.0f, D2D1::ColorF(0x6B7280)));
    popupStack->AddChild(popupStatus);
    popupStack->AddChild(popupHost);
    popupCard->AddChild(popupStack);
    root->AddChild(popupCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto comboCard = std::make_shared<Card>(surface, 20.0f);
    auto comboStack = std::make_shared<StackPanel>(14.0f);
    auto comboStatus =
        std::make_shared<TextBlock>(L"Current mode: Core IME", 14.0f, D2D1::ColorF(0x475569), true);
    auto comboBox = std::make_shared<ComboBox>(46.0f);
    comboBox->SetWidth(260.0f);
    comboBox->AddItem(L"Core IME");
    comboBox->AddItem(L"Diagnostics");
    comboBox->AddItem(L"Popup Overlay");
    comboBox->AddItem(L"Experimental Theme");
    comboBox->SetOnSelectionChanged([comboStatus](size_t selectedIndex, const std::wstring &value) {
        (void)selectedIndex;
        comboStatus->SetText(L"Current mode: " + value);
    });

    comboStack->AddChild(std::make_shared<TextBlock>(L"ComboBox", 20.0f, D2D1::ColorF(0x1F2937), true));
    comboStack->AddChild(std::make_shared<TextBlock>(
        L"The first drop-down selector is now built on top of the overlay popup path, so selection controls can expand outside normal layout bounds.",
        14.0f, D2D1::ColorF(0x6B7280)));
    comboStack->AddChild(comboStatus);
    comboStack->AddChild(comboBox);
    comboCard->AddChild(comboStack);
    root->AddChild(comboCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto candidateCard = std::make_shared<Card>(surface, 20.0f);
    auto candidateStack = std::make_shared<StackPanel>(14.0f);
    auto candidateStatus =
        std::make_shared<TextBlock>(L"Selected candidate: 1 \x4F60(rX)", 14.0f, D2D1::ColorF(0x475569), true);
    auto candidateTrigger = std::make_shared<Button>(L"Open Candidate Preview", 44.0f);
    candidateTrigger->SetWidth(220.0f);

    auto candidatePopupStack = std::make_shared<StackPanel>(10.0f);
    candidatePopupStack->SetHorizontalContentAlignment(HorizontalAlignment::Leading);
    candidatePopupStack->AddChild(std::make_shared<TextBlock>(L"ni", 20.0f, D2D1::ColorF(0xFFFFFF), true));

    auto candidateList = std::make_shared<CandidateList>(30.0f);
    candidateList->SetWidth(124.0f);
    candidateList->AddItem({L"1", L"\x4F60", L"(rX)"});
    candidateList->AddItem({L"2", L"\x5C3C", L"(uV)"});
    candidateList->AddItem({L"3", L"\x59AE", L"(nV)"});
    candidateList->AddItem({L"4", L"\x6CE5", L"(dV)"});
    candidateList->AddItem({L"5", L"\x9006", L"(zQ)"});
    candidateList->AddItem({L"6", L"\x62DF", L"(fR)"});
    candidateList->AddItem({L"7", L"\x817B", L"(oD)"});
    candidateList->AddItem({L"8", L"\x502A", L"(rE)"});
    candidateList->SetOnSelectionChanged([candidateStatus](size_t selectedIndex) {
        static const wchar_t *labels[] = {L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"};
        static const wchar_t *texts[] = {L"\x4F60", L"\x5C3C", L"\x59AE", L"\x6CE5", L"\x9006", L"\x62DF", L"\x817B", L"\x502A"};
        static const wchar_t *annotations[] = {L"(rX)", L"(uV)", L"(nV)", L"(dV)", L"(zQ)", L"(fR)", L"(oD)", L"(rE)"};
        if (selectedIndex < 8)
        {
            candidateStatus->SetText(std::wstring(L"Selected candidate: ") + labels[selectedIndex] + L" " +
                                     texts[selectedIndex] + annotations[selectedIndex]);
        }
    });
    candidatePopupStack->AddChild(candidateList);

    auto candidatePopup = std::make_shared<Popup>(candidatePopupStack);
    candidatePopup->SetMatchAnchorWidth(false);
    candidatePopup->SetWidth(136.0f);
    candidatePopup->SetPadding({8.0f, 8.0f, 8.0f, 8.0f});
    candidatePopup->SetBackgroundFill(D2D1::ColorF(0x242424));
    candidatePopup->SetBorderColor(D2D1::ColorF(0x474747));
    candidatePopup->SetCornerRadius(12.0f);
    candidatePopup->SetShadowEnabled(true);
    candidatePopup->SetOffset(0.0f, 6.0f);

    auto candidateHost = std::make_shared<PopupHost>(candidateTrigger, candidatePopup);

    candidateStack->AddChild(std::make_shared<TextBlock>(L"Candidate Window Target", 20.0f, D2D1::ColorF(0x1F2937), true));
    candidateStack->AddChild(std::make_shared<TextBlock>(
        L"This is the first candidate-window-oriented preview for replacing the WebView2 IME candidate popup with native retained-mode rendering.",
        14.0f, D2D1::ColorF(0x6B7280)));
    candidateStack->AddChild(candidateStatus);
    candidateStack->AddChild(candidateHost);
    candidateCard->AddChild(candidateStack);
    root->AddChild(candidateCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto contextMenuCard = std::make_shared<Card>(surface, 20.0f);
    auto contextMenuStack = std::make_shared<StackPanel>(14.0f);
    auto contextMenuStatus =
        std::make_shared<TextBlock>(L"Last action: none", 14.0f, D2D1::ColorF(0x475569), true);

    auto contextMenuPreviewText = std::make_shared<TextBlock>(
        L"Right-click this surface to open a context menu. This is the first step toward file-tree menus, list item actions, and editor command menus.",
        14.0f, D2D1::ColorF(0x475569));
    auto contextMenuPreview = std::make_shared<Border>(insetBrush, contextMenuPreviewText);
    contextMenuPreview->SetPadding({16.0f, 18.0f, 16.0f, 18.0f});
    contextMenuPreview->SetHeight(92.0f);

    auto contextMenuPopupStack = std::make_shared<StackPanel>(8.0f);
    contextMenuPopupStack->AddChild(std::make_shared<TextBlock>(L"Canvas Actions", 15.0f, D2D1::ColorF(0x1F2937), true));
    auto contextMenuRefresh = std::make_shared<Button>(L"Refresh Preview", 38.0f);
    auto contextMenuInspect = std::make_shared<Button>(L"Inspect Layout", 38.0f);
    auto contextMenuPin = std::make_shared<Button>(L"Pin as Favorite", 38.0f);
    contextMenuRefresh->SetWidth(180.0f);
    contextMenuInspect->SetWidth(180.0f);
    contextMenuPin->SetWidth(180.0f);
    contextMenuPopupStack->AddChild(contextMenuRefresh);
    contextMenuPopupStack->AddChild(contextMenuInspect);
    contextMenuPopupStack->AddChild(contextMenuPin);

    auto contextMenuPopup = std::make_shared<Popup>(contextMenuPopupStack);
    contextMenuPopup->SetMatchAnchorWidth(false);
    contextMenuPopup->SetWidth(220.0f);
    contextMenuPopup->SetOffset(0.0f, 4.0f);

    auto contextMenuHost = std::make_shared<ContextMenuHost>(contextMenuPreview, contextMenuPopup);
    std::weak_ptr<ContextMenuHost> weakContextMenuHost = contextMenuHost;
    contextMenuRefresh->SetOnClick([contextMenuStatus, weakContextMenuHost]() {
        contextMenuStatus->SetText(L"Last action: refresh preview");
        if (auto host = weakContextMenuHost.lock())
        {
            host->ClosePopup();
        }
    });
    contextMenuInspect->SetOnClick([contextMenuStatus, weakContextMenuHost]() {
        contextMenuStatus->SetText(L"Last action: inspect layout");
        if (auto host = weakContextMenuHost.lock())
        {
            host->ClosePopup();
        }
    });
    contextMenuPin->SetOnClick([contextMenuStatus, weakContextMenuHost]() {
        contextMenuStatus->SetText(L"Last action: pin as favorite");
        if (auto host = weakContextMenuHost.lock())
        {
            host->ClosePopup();
        }
    });

    contextMenuStack->AddChild(std::make_shared<TextBlock>(L"ContextMenuHost", 20.0f, D2D1::ColorF(0x1F2937), true));
    contextMenuStack->AddChild(std::make_shared<TextBlock>(
        L"Right-click menus can now reuse the same scene overlay layer, so command lists no longer need to stay inside normal layout bounds.",
        14.0f, D2D1::ColorF(0x6B7280)));
    contextMenuStack->AddChild(contextMenuStatus);
    contextMenuStack->AddChild(contextMenuHost);
    contextMenuCard->AddChild(contextMenuStack);
    root->AddChild(contextMenuCard);
    root->AddChild(std::make_shared<Spacer>(24.0f));

    auto accordionCard = std::make_shared<Card>(surface, 20.0f);
    auto accordionStack = std::make_shared<StackPanel>(14.0f);
    auto accordion = std::make_shared<Accordion>(46.0f);
    accordion->SetAllowMultipleExpanded(true);

    auto basicsPanel = std::make_shared<StackPanel>(10.0f);
    basicsPanel->AddChild(std::make_shared<TextBlock>(
        L"Basic controls include buttons, checkboxes, sliders, text blocks, and progress indicators.",
        14.0f, D2D1::ColorF(0x475569)));
    basicsPanel->AddChild(std::make_shared<Button>(L"Basic Action", 38.0f));

    auto layoutSectionPanel = std::make_shared<StackPanel>(10.0f);
    layoutSectionPanel->AddChild(std::make_shared<TextBlock>(
        L"Layout now covers stack, wrap, grid, scrolling, sizing constraints, and container alignment.",
        14.0f, D2D1::ColorF(0x475569)));

    auto navSectionPanel = std::make_shared<StackPanel>(10.0f);
    navSectionPanel->AddChild(std::make_shared<TextBlock>(
        L"Navigation includes ListView, TreeView, and TabControl for higher-level page composition.",
        14.0f, D2D1::ColorF(0x475569)));
    auto navSectionBorder = std::make_shared<Border>(insetBrush, std::make_shared<TextBlock>(
                                                                    L"Accordion is useful when you want one long page with grouped expandable sections instead of switching tabs.",
                                                                    13.0f, D2D1::ColorF(0x475569)));
    navSectionBorder->SetPadding({14.0f, 12.0f, 14.0f, 12.0f});
    navSectionPanel->AddChild(navSectionBorder);

    accordion->AddSection(L"Basic Controls", basicsPanel, true);
    accordion->AddSection(L"Layout System", layoutSectionPanel, false);
    accordion->AddSection(L"Navigation & Containers", navSectionPanel, false);

    accordionStack->AddChild(std::make_shared<TextBlock>(L"Accordion", 20.0f, D2D1::ColorF(0x1F2937), true));
    accordionStack->AddChild(std::make_shared<TextBlock>(
        L"A first-pass accordion container is now available for long settings pages and grouped expandable content areas.",
        14.0f, D2D1::ColorF(0x6B7280)));
    accordionStack->AddChild(accordion);
    accordionCard->AddChild(accordionStack);
    root->AddChild(accordionCard);
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

    auto rootContent = std::make_shared<Container>(root);
    rootContent->SetPadding({5.0f, 0.0f, 0.0f, 5.0f});

    auto scene = std::make_unique<Scene>();
    scene->SetRoot(std::make_shared<ScrollViewer>(rootContent));
    return scene;
}
} // namespace msimeui
