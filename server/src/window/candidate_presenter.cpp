#include "window/candidate_presenter.h"

#include "config/ime_config.h"
#include "defines/defines.h"
#include "defines/globals.h"
#include "global/globals.h"
#include "ipc/ipc.h"
#include "log/candidate_diag_log.h"
#include "skin/candidate_skin_catalog.h"
#include "utils/common_utils.h"
#include "utils/ime_utils.h"
#include "utils/window_utils.h"
#include "window/ime_windows.h"

#include "msimeui/Controls.h"
#include "msimeui/DeviceResources.h"
#include "msimeui/Layout.h"
#include "msimeui/Scene.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"

#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/core/scheme_type.h"
#include "MetasequoiaImeEngine/core/word_item.h"

#include <d2d1.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
D2D1_COLOR_F ColorFromRgb(UINT rgb, float alpha = 1.0f)
{
    return D2D1::ColorF(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
}

std::string TrimCopy(std::string text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
        text.pop_back();
    }
    return text;
}

int AlignHostPixels(int value)
{
    constexpr int kBucket = 64;
    if (value < 1)
    {
        value = 1;
    }
    return ((value + kBucket - 1) / kBucket) * kBucket;
}

D2D1_COLOR_F ParseCssColor(const std::string &text, D2D1_COLOR_F fallback)
{
    std::string value = TrimCopy(text);
    if (value.empty() || value == "auto" || value == "none" || value == "transparent")
    {
        return value == "transparent" ? D2D1::ColorF(0, 0.0f) : fallback;
    }
    if (value.rfind("rgba(", 0) == 0 || value.rfind("rgb(", 0) == 0)
    {
        const auto open = value.find('(');
        const auto close = value.rfind(')');
        if (open != std::string::npos && close != std::string::npos && close > open)
        {
            std::string inner = value.substr(open + 1, close - open - 1);
            for (char &ch : inner)
            {
                if (ch == ',')
                {
                    ch = ' ';
                }
            }
            std::istringstream stream(inner);
            float r = 0;
            float g = 0;
            float b = 0;
            float a = 1.0f;
            if (stream >> r >> g >> b)
            {
                stream >> a;
                return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a);
            }
        }
        return fallback;
    }
    if (value[0] == '#')
    {
        value.erase(value.begin());
    }
    auto hexByte = [](const std::string &hex) {
        return static_cast<int>(std::stoul(hex, nullptr, 16));
    };
    try
    {
        if (value.size() == 3)
        {
            const int r = hexByte(std::string(2, value[0]));
            const int g = hexByte(std::string(2, value[1]));
            const int b = hexByte(std::string(2, value[2]));
            return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        }
        if (value.size() == 6)
        {
            return ColorFromRgb(static_cast<UINT>(std::stoul(value, nullptr, 16)));
        }
        if (value.size() == 8)
        {
            const unsigned long packed = std::stoul(value, nullptr, 16);
            const UINT rgb = static_cast<UINT>((packed >> 8) & 0xFFFFFFu);
            const float alpha = static_cast<float>(packed & 0xFFu) / 255.0f;
            return ColorFromRgb(rgb, alpha);
        }
    }
    catch (...)
    {
    }
    return fallback;
}

struct CandSkinTokens
{
    D2D1_COLOR_F surface = ColorFromRgb(0x202020);
    D2D1_COLOR_F border = ParseCssColor("#9b9b9b2e", ColorFromRgb(0x3A3A3A, 0.18f));
    D2D1_COLOR_F text = ParseCssColor("#e9e8e8", ColorFromRgb(0xE9E8E8));
    D2D1_COLOR_F number = ParseCssColor("#e9e8e89d", ColorFromRgb(0xE9E8E8, 0.616f));
    D2D1_COLOR_F selected = ParseCssColor("#3e3e3eb9", ColorFromRgb(0x3E3E3E, 0.725f));
    D2D1_COLOR_F hover = ColorFromRgb(0x414141);
    D2D1_COLOR_F accent = ColorFromRgb(0x6B69D6);
    float radius = 6.0f;
    float borderWidth = 1.5f;
    float containerPad = 5.0f;
    bool showSelectedBar = true;
};

void ApplyPackageColors(const CandidateSkinCatalog::CandidateColors &colors, CandSkinTokens &tokens)
{
    if (!colors.accent.empty())
    {
        tokens.accent = ParseCssColor(colors.accent, tokens.accent);
    }
    if (!colors.selected.empty())
    {
        tokens.selected = ParseCssColor(colors.selected, tokens.selected);
    }
    if (!colors.hover.empty())
    {
        tokens.hover = ParseCssColor(colors.hover, tokens.hover);
    }
    if (!colors.surface.empty())
    {
        tokens.surface = ParseCssColor(colors.surface, tokens.surface);
    }
    if (!colors.border.empty())
    {
        tokens.border = ParseCssColor(colors.border, tokens.border);
    }
    if (!colors.text.empty())
    {
        tokens.text = ParseCssColor(colors.text, tokens.text);
    }
    if (!colors.number.empty())
    {
        tokens.number = ParseCssColor(colors.number, tokens.number);
    }
    if (colors.showSelectedBar.has_value())
    {
        tokens.showSelectedBar = *colors.showSelectedBar;
    }
}

std::wstring AssetRoot()
{
    return string_to_wstring(CommonUtils::get_local_appdata_path()) + L"\\" + GlobalIme::AppName;
}

constexpr float kShadowPadLeft = 32.0f;
constexpr float kShadowPadTop = 20.0f;
constexpr float kShadowPadRight = 32.0f;
constexpr float kShadowPadBottom = 40.0f;
constexpr float kCandidateMinWidthDip = 160.0f;
constexpr float kDecorationCardOverlap = 5.0f;

bool ShouldShowHelpcode()
{
    switch (GetConfiguredActiveInputScheme())
    {
    case SchemeType::Shuangpin:
        return GetConfiguredShowShuangpinHelpcodeInCandidateWindow();
    case SchemeType::Quanpin:
        return GetConfiguredShowQuanpinHelpcodeInCandidateWindow();
    default:
        return false;
    }
}
} // namespace

struct CandidatePresenter::Impl
{
    msimeui::DeviceResources resources;
    std::unique_ptr<msimeui::Window> window;
    std::shared_ptr<msimeui::StackPanel> root;
    std::shared_ptr<msimeui::Image> decoration;
    std::shared_ptr<msimeui::Card> card;
    std::shared_ptr<msimeui::Container> frame;
    std::shared_ptr<msimeui::StackPanel> body;
    std::shared_ptr<msimeui::TextBlock> preedit;
    std::shared_ptr<msimeui::CandidateList> list;
    std::shared_ptr<msimeui::Popup> contextMenu;
    std::shared_ptr<msimeui::Popup> contextSubmenu;
    std::shared_ptr<msimeui::MenuFlyoutItem> fixPositionItem;
    D2D1_COLOR_F menuFill = D2D1::ColorF(0x2D2D2D);
    D2D1_COLOR_F menuBorder = ParseCssColor("#9b9b9b2e", D2D1::ColorF(0x3A3A3A, 0.18f));
    D2D1_COLOR_F menuText = D2D1::ColorF(0xE9E8E8);
    D2D1_COLOR_F menuHover = D2D1::ColorF(0x414141);
    RECT hostRectBeforeMenu{};
    bool contextMenuOpen = false;
    bool contextSubmenuOpen = false;
    bool hostExpandedForMenu = false;
    size_t contextMenuPageIndex = 0;
};

CandidatePresenter::CandidatePresenter() = default;

CandidatePresenter &CandidatePresenter::Instance()
{
    static CandidatePresenter instance;
    return instance;
}

bool CandidatePresenter::IsBound() const
{
    return bound_;
}

bool CandidatePresenter::Bind(HWND hwnd)
{
    hwnd_ = hwnd;
    if (!hwnd)
    {
        bound_ = false;
        impl_.reset();
        return false;
    }
    impl_ = std::make_unique<Impl>();
    impl_->window = std::make_unique<msimeui::Window>(L"metasequoiaime_windows", L"cand", 1, 1);
    impl_->window->AdoptExistingHwnd(hwnd);
    impl_->window->SetStealFocusOnClick(false);
    bound_ = impl_->resources.EnsureForComposition(hwnd);
    RebuildScene();
    CAND_DIAG_LOGF(L"candidate-d2d bind hwnd={:#x} composition={}", reinterpret_cast<uintptr_t>(hwnd),
                   bound_ ? 1 : 0);
    return bound_;
}

void CandidatePresenter::RebuildScene()
{
    if (!impl_ || !impl_->window)
    {
        return;
    }
    lastSkinFingerprint_.clear();
    impl_->root = std::make_shared<msimeui::StackPanel>(0.0f);
    impl_->decoration = std::make_shared<msimeui::Image>(L"");
    impl_->decoration->SetHorizontalAlignment(msimeui::HorizontalAlignment::Trailing);
    impl_->preedit = std::make_shared<msimeui::TextBlock>(L"", 14.0f, D2D1::ColorF(0xF5F5F5));
    impl_->preedit->SetTextLayoutPadding({0.0f, 1.0f, 0.0f, 1.0f});
    impl_->preedit->SetHorizontalAlignment(msimeui::HorizontalAlignment::Leading);
    impl_->list = std::make_shared<msimeui::CandidateList>(28.0f);
    impl_->list->SetOnItemActivated([this](size_t index) { CommitItem(index); });
    impl_->list->SetOnContextMenu([this](size_t index, const POINT &clientPoint) {
        ShowItemContextMenu(index, clientPoint);
    });
    impl_->body = std::make_shared<msimeui::StackPanel>(2.0f);
    impl_->body->SetPadding({0.0f, 0.0f, 0.0f, 0.0f});
    impl_->body->AddChild(impl_->preedit);
    impl_->body->AddChild(impl_->list);
    msimeui::Brush brush;
    brush.fill = D2D1::ColorF(0x202020);
    brush.stroke = ParseCssColor("#9b9b9b2e", D2D1::ColorF(0x3A3A3A, 0.18f));
    brush.strokeWidth = 1.5f;
    brush.radiusX = 6.0f;
    brush.radiusY = 6.0f;
    impl_->card = std::make_shared<msimeui::Card>(brush, 5.0f);
    impl_->card->SetHorizontalAlignment(msimeui::HorizontalAlignment::Leading);
    impl_->card->AddChild(impl_->body);
    impl_->root->SetHorizontalContentAlignment(msimeui::HorizontalAlignment::Leading);
    impl_->frame = std::make_shared<msimeui::Container>();
    impl_->frame->SetPadding({kShadowPadLeft, kShadowPadTop, kShadowPadRight, kShadowPadBottom});
    impl_->frame->SetChild(impl_->card);
    impl_->root->AddChild(impl_->decoration);
    impl_->root->AddChild(impl_->frame);
    auto scene = std::make_unique<msimeui::Scene>();
    scene->SetRoot(impl_->root);
    impl_->window->SetScene(std::move(scene));
}

void CandidatePresenter::ApplySkin()
{
    if (!impl_ || !impl_->list || !impl_->preedit || !impl_->window)
    {
        return;
    }
    const std::string skinId = GetConfiguredCandidateSkin();
    std::ostringstream fingerprint;
    fingerprint << skinId << '|' << GetConfiguredThemeCand() << '|' << GetConfiguredCandidateFont() << '|'
                << GetConfiguredCandidateFontSize() << '|' << GetConfiguredCandidateWindowPreeditFontSize() << '|'
                << GetConfiguredCandidateWindowLayout() << '|' << GetConfiguredCandidateTextColor();
    const std::string skinKey = fingerprint.str();
    if (skinKey == lastSkinFingerprint_ && impl_->card)
    {
        return;
    }
    lastSkinFingerprint_ = skinKey;

    const bool candLight = ResolveConfiguredTheme(GetConfiguredThemeCand()) == "light";
    CandSkinTokens tokens;
    if (candLight)
    {
        tokens.surface = ColorFromRgb(0xFFFFFF);
        tokens.border = D2D1::ColorF(0, 0.12f);
        tokens.text = ColorFromRgb(0x1A1A1A);
        tokens.number = D2D1::ColorF(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 0.55f);
        tokens.selected = ColorFromRgb(0xE8E8E8);
        tokens.hover = ColorFromRgb(0xECECEC);
    }
    if (skinId == "wechat")
    {
        tokens.surface = candLight ? ColorFromRgb(0xF7F7F7) : ColorFromRgb(0x151515);
        tokens.border = ColorFromRgb(0x292929);
        tokens.borderWidth = 1.0f;
        tokens.radius = 5.0f;
        tokens.accent = ColorFromRgb(0x07C160);
        tokens.selected = ColorFromRgb(0x07C160);
        tokens.hover = D2D1::ColorF(7.0f / 255.0f, 193.0f / 255.0f, 96.0f / 255.0f, 0.32f);
        tokens.showSelectedBar = false;
        tokens.text = ColorFromRgb(0xB7B7B7);
        tokens.number = ColorFromRgb(0x858585);
    }
    else if (skinId == "willow_green")
    {
        tokens.surface = ColorFromRgb(0x2D2F2E);
        tokens.borderWidth = 0.0f;
        tokens.radius = 9.0f;
        tokens.containerPad = 0.0f;
        tokens.accent = ColorFromRgb(0x65C98D);
        tokens.selected = ColorFromRgb(0x65C98D);
        tokens.hover = D2D1::ColorF(101.0f / 255.0f, 201.0f / 255.0f, 141.0f / 255.0f, 0.22f);
        tokens.text = ColorFromRgb(0xD8DBD8);
        tokens.number = ColorFromRgb(0xA6ABA7);
        tokens.showSelectedBar = false;
    }
    else if (skinId == "graphite")
    {
        tokens.surface = ColorFromRgb(0x1C1F23);
        tokens.border = ColorFromRgb(0x30353B);
        tokens.borderWidth = 1.0f;
        tokens.radius = 3.0f;
        tokens.containerPad = 5.0f;
        tokens.accent = ColorFromRgb(0x8993A0);
        tokens.selected = D2D1::ColorF(0, 0.0f);
        tokens.hover = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.055f);
        tokens.text = ColorFromRgb(0xAEB6C2);
        tokens.number = ColorFromRgb(0x707987);
        tokens.showSelectedBar = false;
    }

    const std::wstring skinsRoot = AssetRoot() + L"\\skins";
    std::optional<CandidateSkinCatalog::Package> package;
    if (!CandidateSkinCatalog::IsBuiltIn(skinId))
    {
        package = CandidateSkinCatalog::Load(std::filesystem::path(skinsRoot), skinId);
        if (package)
        {
            ApplyPackageColors(candLight ? package->light : package->dark, tokens);
        }
    }

    msimeui::Theme theme = msimeui::ThemeManager::GetCurrent();
    theme.textInputFontFamily = string_to_wstring(GetConfiguredCandidateFont());
    if (theme.textInputFontFamily.empty())
    {
        theme.textInputFontFamily = L"Microsoft YaHei UI";
    }
    theme.uiFontFamily = theme.textInputFontFamily;
    theme.surface = tokens.surface;
    theme.border = tokens.border;
    theme.textPrimary = ParseCssColor(GetConfiguredCandidateTextColor(), tokens.text);
    theme.textSecondary = tokens.number;
    theme.primary = tokens.accent;
    theme.windowBackground = D2D1::ColorF(0, 0.0f);
    msimeui::ThemeManager::SetCurrent(theme);

    const float fontSize = static_cast<float>((std::max)(12, GetConfiguredCandidateFontSize()));
    const float preeditSize = static_cast<float>((std::max)(12, GetConfiguredCandidateWindowPreeditFontSize()));
    msimeui::CandidateList::Appearance appearance;
    appearance.itemHeight = fontSize * 1.35f + 2.0f;
    appearance.itemGap = 2.0f;
    appearance.fontSize = fontSize;
    appearance.labelFontSize = fontSize * 0.8f;
    appearance.annotationFontSize = fontSize;
    appearance.cornerRadius = 4.0f;
    appearance.contentPadLeft = 0.0f;
    appearance.contentPadRight = 0.0f;
    appearance.textPadLeft = 5.0f;
    appearance.labelGap = 1.5f;
    appearance.selectedBarWidth = 3.0f;
    appearance.selectedBarHeight = fontSize * 0.85f;
    appearance.showSelectedBar = tokens.showSelectedBar;
    appearance.selectedBarColor = tokens.accent;
    appearance.textColor = theme.textPrimary;
    appearance.labelColor = tokens.number;
    appearance.annotationColor = theme.textPrimary;
    appearance.rowFillSelected = tokens.selected;
    appearance.rowFillHover = tokens.hover;
    appearance.rowFillPressed = tokens.selected;
    impl_->menuFill = candLight ? ColorFromRgb(0xFFFFFF) : ColorFromRgb(0x2D2D2D);
    impl_->menuBorder = tokens.border;
    impl_->menuText = theme.textPrimary;
    impl_->menuHover = tokens.hover;
    impl_->list->SetAppearance(appearance);
    impl_->list->SetOrientation(GetConfiguredCandidateWindowLayout() == "horizontal"
                                    ? msimeui::CandidateList::Orientation::Horizontal
                                    : msimeui::CandidateList::Orientation::Vertical);
    impl_->preedit->SetFontFamily(theme.textInputFontFamily);
    impl_->preedit->SetFontSize(preeditSize);
    impl_->preedit->SetColor(theme.textPrimary);
    impl_->preedit->SetCaretColor(tokens.accent);
    impl_->preedit->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    impl_->preedit->SetTextLayoutPadding({5.0f, 0.0f, 5.0f, 0.0f});

    decorationTopDip_ = 0.0f;
    decorationWidthDip_ = 0.0f;
    std::wstring decorationPath;
    if (package)
    {
        decorationTopDip_ = static_cast<float>(package->decorationTopDip);
        decorationWidthDip_ = static_cast<float>(package->decorationWidthDip);
        if (!package->preview.empty())
        {
            decorationPath = skinsRoot + L"\\" + string_to_wstring(package->id) + L"\\" +
                             string_to_wstring(package->preview);
        }
    }
    if (decorationPath.empty() || decorationTopDip_ <= 0.0f)
    {
        impl_->decoration->SetHeight(0.0f);
        impl_->decoration->ClearWidth();
        impl_->decoration->SetSource(L"");
        impl_->decoration->SetMargin({0.0f, 0.0f, 0.0f, 0.0f});
    }
    else
    {
        impl_->decoration->SetSource(decorationPath);
        impl_->decoration->SetHeight(decorationTopDip_);
        if (decorationWidthDip_ > 0.0f)
        {
            impl_->decoration->SetWidth(decorationWidthDip_);
        }
        impl_->decoration->SetStretch(msimeui::ImageStretch::Uniform);
        impl_->decoration->SetMargin(
            {0.0f, 0.0f, 0.0f, -(kShadowPadTop + kDecorationCardOverlap)});
    }

    msimeui::Brush brush;
    brush.fill = tokens.surface;
    brush.stroke = tokens.border;
    brush.strokeWidth = tokens.borderWidth;
    brush.radiusX = tokens.radius;
    brush.radiusY = tokens.radius;
    if (impl_->card)
    {
        impl_->card->SetBrush(brush);
        impl_->card->SetPadding(tokens.containerPad);
        impl_->card->SetMinWidth(kCandidateMinWidthDip);
    }
}

void CandidatePresenter::FillItemsFromUi()
{
    auto &ui = Global::candidate_ui;
    std::vector<msimeui::CandidateList::Item> items;
    const bool help = ShouldShowHelpcode();
    const int start = ui.current_page_start();
    for (size_t i = 0; i < ui.page_words.size(); ++i)
    {
        msimeui::CandidateList::Item item;
        item.label = std::to_wstring(i + 1);
        item.text = ui.page_words[i];
        const size_t absIndex = static_cast<size_t>(start) + i;
        if (absIndex < ui.items.size())
        {
            switch (ui.items[absIndex].source)
            {
            case CandidateSource::CloudSuggestion:
                item.text += L" ☁️";
                break;
            case CandidateSource::AiSuggestion:
                item.text += L" 🤖";
                break;
            default:
                break;
            }
        }
        if (help && absIndex < ui.items.size())
        {
            item.annotation = string_to_wstring(HelpcodeUtils::compute_helpcodes(ui.items[absIndex].word));
        }
        if (GetConfiguredCandidateTranslationsEnabled() && GetConfiguredCandidateWindowLayout() == "vertical" &&
            i < ui.page_glosses.size())
        {
            item.translation = ui.page_glosses[i];
        }
        items.push_back(std::move(item));
    }
    ignoreSelectionCallback_ = true;
    impl_->list->SetItems(std::move(items));
    impl_->list->SetSelectedIndex(static_cast<size_t>((std::max)(0, ui.selected_index_in_page)));
    ignoreSelectionCallback_ = false;
}

void CandidatePresenter::CommitItem(size_t pageIndex)
{
    if (!hwnd_)
    {
        return;
    }
    PostMessageW(hwnd_, WM_COMMIT_CANDIDATE, static_cast<WPARAM>(pageIndex + 1), 0);
}

namespace
{
constexpr float kContextMenuWidthDip = 108.0f;
constexpr float kContextSubmenuWidthDip = 96.0f;

void StyleContextPopup(msimeui::Popup &popup, const D2D1_COLOR_F &fill, const D2D1_COLOR_F &border, float width)
{
    popup.SetMatchAnchorWidth(false);
    popup.SetWidth(width);
    popup.SetPadding({2.0f, 2.0f, 2.0f, 2.0f});
    popup.SetBackgroundFill(fill);
    popup.SetBorderColor(border);
    popup.SetCornerRadius(6.0f);
    popup.SetShadowEnabled(true);
    popup.SetOffset(0.0f, 0.0f);
    popup.SetConstrainToViewport(true);
}
} // namespace

void CandidatePresenter::ShowItemContextMenu(size_t pageIndex, POINT clientPoint)
{
    if (!hwnd_ || !impl_ || !impl_->list || !impl_->window)
    {
        return;
    }

    CloseContextMenu(true);
    impl_->contextMenuPageIndex = pageIndex;

    auto &ui = Global::candidate_ui;
    std::wstring word;
    if (pageIndex < ui.page_words.size())
    {
        word = ui.page_words[pageIndex];
    }
    size_t codePoints = 0;
    for (size_t i = 0; i < word.size(); ++i)
    {
        ++codePoints;
        if (i + 1 < word.size() && IS_HIGH_SURROGATE(word[i]) && IS_LOW_SURROGATE(word[i + 1]))
        {
            ++i;
        }
    }
    const bool showDelete = codePoints != 1;
    const WPARAM oneBased = static_cast<WPARAM>(pageIndex + 1);

    auto makeItem = [this](const std::wstring &text, bool hasSubmenu, std::function<void()> onClick) {
        auto item = std::make_shared<msimeui::MenuFlyoutItem>(text, hasSubmenu);
        item->SetColors(impl_->menuText, impl_->menuHover);
        if (onClick)
        {
            item->SetOnClick(std::move(onClick));
        }
        return item;
    };

    auto stack = std::make_shared<msimeui::StackPanel>(0.0f);
    auto pin = makeItem(L"置顶", false, [this, oneBased]() {
        CloseContextMenu(true);
        PostMessageW(hwnd_, WM_PIN_TO_TOP_CANDIDATE, oneBased, 0);
    });
    pin->SetOnHover([this](bool hovered) {
        if (hovered)
        {
            CloseFixSubmenu();
        }
    });
    impl_->fixPositionItem = makeItem(L"固定排位", true, {});
    impl_->fixPositionItem->SetOnHover([this](bool hovered) {
        if (hovered)
        {
            OpenFixSubmenu();
        }
    });
    stack->AddChild(pin);
    stack->AddChild(impl_->fixPositionItem);
    if (showDelete)
    {
        auto del = makeItem(L"删除", false, [this, oneBased]() {
            CloseContextMenu(true);
            PostMessageW(hwnd_, WM_DELETE_CANDIDATE, oneBased, 0);
        });
        del->SetOnHover([this](bool hovered) {
            if (hovered)
            {
                CloseFixSubmenu();
            }
        });
        stack->AddChild(del);
    }

    auto subStack = std::make_shared<msimeui::StackPanel>(0.0f);
    for (int position = 1; position <= 5; ++position)
    {
        const std::wstring label = L"第 " + std::to_wstring(position) + L" 位";
        subStack->AddChild(makeItem(label, false, [this, oneBased, position]() {
            CloseContextMenu(true);
            PostMessageW(hwnd_, WM_FIX_CANDIDATE_POSITION, oneBased, position);
        }));
    }
    auto separator = std::make_shared<msimeui::MenuSeparator>();
    separator->SetColor(D2D1::ColorF(impl_->menuText.r, impl_->menuText.g, impl_->menuText.b, 0.125f));
    subStack->AddChild(separator);
    subStack->AddChild(makeItem(L"取消固定", false, [this, oneBased]() {
        CloseContextMenu(true);
        PostMessageW(hwnd_, WM_CLEAR_CANDIDATE_POSITION, oneBased, 0);
    }));

    impl_->contextMenu = std::make_shared<msimeui::Popup>(stack);
    StyleContextPopup(*impl_->contextMenu, impl_->menuFill, impl_->menuBorder, kContextMenuWidthDip);
    impl_->contextSubmenu = std::make_shared<msimeui::Popup>(subStack);
    StyleContextPopup(*impl_->contextSubmenu, impl_->menuFill, impl_->menuBorder, kContextSubmenuWidthDip);

    const msimeui::PointF anchor = impl_->window->ClientPixelsToDips(clientPoint);
    impl_->contextMenu->SetAnchorRect({anchor.x, anchor.y, 1.0f, 1.0f});
    ExpandHostForMenu(clientPoint);

    if (msimeui::Scene *scene = impl_->window->GetScene())
    {
        scene->AddPopup(impl_->contextMenu, [this]() { CloseContextMenu(true); });
        impl_->contextMenuOpen = true;
    }
    Present();
}

void CandidatePresenter::CloseContextMenu(bool restoreHost)
{
    if (!impl_ || !impl_->window)
    {
        return;
    }

    if (msimeui::Scene *scene = impl_->window->GetScene())
    {
        if (impl_->contextSubmenu)
        {
            scene->RemovePopup(impl_->contextSubmenu.get(), false);
        }
        if (impl_->contextMenu)
        {
            scene->RemovePopup(impl_->contextMenu.get(), false);
        }
    }
    impl_->contextMenuOpen = false;
    impl_->contextSubmenuOpen = false;
    impl_->fixPositionItem.reset();
    impl_->contextMenu.reset();
    impl_->contextSubmenu.reset();
    if (restoreHost)
    {
        RestoreHostAfterMenu();
    }
    else
    {
        impl_->hostExpandedForMenu = false;
    }
}

void CandidatePresenter::OpenFixSubmenu()
{
    if (!impl_ || !impl_->window || !impl_->contextSubmenu || !impl_->fixPositionItem || impl_->contextSubmenuOpen)
    {
        return;
    }
    const msimeui::RectF anchor = impl_->fixPositionItem->GetBounds();
    if (anchor.width < 1.0f || anchor.height < 1.0f)
    {
        return;
    }
    impl_->contextSubmenu->SetAnchorRect(anchor);
    impl_->contextSubmenu->SetOffset(anchor.width - 2.0f, -anchor.height);
    if (msimeui::Scene *scene = impl_->window->GetScene())
    {
        scene->AddPopup(impl_->contextSubmenu, [this]() { impl_->contextSubmenuOpen = false; });
        impl_->contextSubmenuOpen = true;
    }
}

void CandidatePresenter::CloseFixSubmenu()
{
    if (!impl_ || !impl_->window || !impl_->contextSubmenu || !impl_->contextSubmenuOpen)
    {
        return;
    }
    if (msimeui::Scene *scene = impl_->window->GetScene())
    {
        scene->RemovePopup(impl_->contextSubmenu.get(), false);
    }
    impl_->contextSubmenuOpen = false;
}

void CandidatePresenter::ExpandHostForMenu(POINT clientPoint)
{
    if (!hwnd_ || !impl_ || !impl_->window)
    {
        return;
    }
    RECT windowRect{};
    RECT clientRect{};
    GetWindowRect(hwnd_, &windowRect);
    GetClientRect(hwnd_, &clientRect);
    if (!impl_->hostExpandedForMenu)
    {
        impl_->hostRectBeforeMenu = windowRect;
        impl_->hostExpandedForMenu = true;
    }

    const float dpi = impl_->window->GetDpi();
    const msimeui::PointF anchor = impl_->window->ClientPixelsToDips(clientPoint);
    const msimeui::SizeF clientDip = impl_->window->ClientPixelsToDips(
        SIZE{clientRect.right, clientRect.bottom});
    const float needWidth = (std::max)(clientDip.width, anchor.x + kContextMenuWidthDip + kContextSubmenuWidthDip + 16.0f);
    const float needHeight = (std::max)(clientDip.height, (std::max)(anchor.y + 168.0f, 176.0f));
    const int widthPx = (std::max)(static_cast<int>(windowRect.right - windowRect.left),
                                   static_cast<int>(std::ceil(needWidth * dpi / 96.0f)));
    const int heightPx = (std::max)(static_cast<int>(windowRect.bottom - windowRect.top),
                                    static_cast<int>(std::ceil(needHeight * dpi / 96.0f)));
    if (widthPx == windowRect.right - windowRect.left && heightPx == windowRect.bottom - windowRect.top)
    {
        return;
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, windowRect.left, windowRect.top, widthPx, heightPx,
                 SWP_NOACTIVATE | SWP_NOZORDER);
}

void CandidatePresenter::RestoreHostAfterMenu()
{
    if (!hwnd_ || !impl_ || !impl_->hostExpandedForMenu)
    {
        return;
    }
    const RECT &rc = impl_->hostRectBeforeMenu;
    SetWindowPos(hwnd_, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    impl_->hostExpandedForMenu = false;
}

void CandidatePresenter::PlaceAndShow(POINT caret, float widthDip, float heightDip, float cardLeftDip,
                                      float cardTopDip)
{
    const HalfScreenDipLimits limits = QueryHalfScreenDipLimitsForPoint(caret);
    FLOAT scale = limits.scale > 0.0f ? limits.scale : GetScaleForPoint(caret);
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }
    widthDip = static_cast<float>(ClampWidthDipToHalfScreen(widthDip, limits));
    heightDip = static_cast<float>(ClampHeightDipToHalfScreen(heightDip, limits));
    lastLayoutWidthDip_ = widthDip;
    lastLayoutHeightDip_ = heightDip;
    cardLeftDip = (std::max)(0.0f, cardLeftDip);
    cardTopDip = (std::max)(0.0f, (std::min)(cardTopDip, heightDip));
    float cardWidthDip = (std::max)(widthDip - cardLeftDip - kShadowPadRight, 1.0f);
    float cardHeightDip = (std::max)(heightDip - cardTopDip - kShadowPadBottom, 1.0f);
    if (impl_->card)
    {
        const msimeui::RectF card = impl_->card->GetBounds();
        if (card.width > 1.0f)
        {
            cardWidthDip = card.width;
        }
        if (card.height > 1.0f)
        {
            cardHeightDip = card.height;
        }
    }
    auto properPos = std::make_shared<std::pair<int, int>>();
    // Position the opaque card, not the decoration or drop shadow around it.
    AdjustCandidateWindowPosition(&caret, {cardWidthDip, cardHeightDip}, properPos, scale, cardWidthDip);
    int widthPx = AlignHostPixels((std::max)(1, static_cast<int>(std::ceil(widthDip * scale))));
    int heightPx = AlignHostPixels((std::max)(1, static_cast<int>(std::ceil(heightDip * scale))));
    widthPx = (std::max)(widthPx, lastHostWidthPx_);
    heightPx = (std::max)(heightPx, lastHostHeightPx_);
    lastHostWidthPx_ = widthPx;
    lastHostHeightPx_ = heightPx;
    const int cardLeftPx = static_cast<int>(std::lround(cardLeftDip * scale));
    const int cardTopPx = static_cast<int>(std::lround(cardTopDip * scale));
    const int cardHeightPx = static_cast<int>(std::lround(cardHeightDip * scale));
    int x = properPos->first - cardLeftPx;
    int y = properPos->second - cardTopPx;
    const MonitorCoordinates monitor = GetMonitorCoordinatesFromPoint(caret);
    if (x + widthPx > monitor.right)
    {
        x = monitor.right - widthPx - 2;
    }
    if (x < monitor.left)
    {
        x = monitor.left + 2;
    }
    const int cardBottom = y + cardTopPx + cardHeightPx;
    if (cardBottom > monitor.bottom)
    {
        y = monitor.bottom - cardTopPx - cardHeightPx - 2;
    }
    // Allow the mascot to clip above the work area so the card stays on the caret.
    if (y + cardTopPx < monitor.top)
    {
        y = monitor.top + 2 - cardTopPx;
    }
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, widthPx, heightPx, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    impl_->resources.EnsureForComposition(hwnd_);
    Present();
    SetCandidateHostCloaked(false);
}

void CandidatePresenter::ShowFromGlobalState()
{
    ShowFromGlobalState(POINT{Global::Point[0], Global::Point[1]});
}

void CandidatePresenter::ShowFromGlobalState(POINT caret)
{
    if (!bound_ || !hwnd_ || !impl_ || !impl_->root)
    {
        return;
    }
    CloseContextMenu(false);
    ApplySkin();
    std::wstring preedit;
    size_t caretIndex = 0;
    if (GetConfiguredCandidateWindowPreeditStyle() != "empty")
    {
        preedit = GetPreeditWithCaretMarker();
        const size_t marker = preedit.find(L'\uE000');
        if (marker != std::wstring::npos)
        {
            caretIndex = marker;
            preedit.erase(marker, 1);
        }
        else
        {
            caretIndex = preedit.size();
        }
    }
    impl_->preedit->SetText(preedit);
    if (preedit.empty() && GetConfiguredCandidateWindowPreeditStyle() == "empty")
    {
        impl_->preedit->ClearCaret();
        impl_->preedit->SetHeight(0.0f);
    }
    else
    {
        impl_->preedit->ClearHeight();
        impl_->preedit->SetCaretIndex(caretIndex);
    }
    FillItemsFromUi();
    impl_->list->SetHoverEnabled(hoverArmed_);
    const HalfScreenDipLimits limits = QueryHalfScreenDipLimitsForPoint(caret);
    const float maxW = limits.maxWidthDip > 1.0 ? static_cast<float>(limits.maxWidthDip) : 480.0f;
    const float maxH = limits.maxHeightDip > 1.0 ? static_cast<float>(limits.maxHeightDip) : 640.0f;
    impl_->root->InvalidateMeasure();
    const msimeui::SizeF measured = impl_->root->MeasureInLayout({maxW, maxH});
    float widthDip = (std::max)(measured.width, kCandidateMinWidthDip + kShadowPadLeft + kShadowPadRight);
    float heightDip = (std::max)(measured.height, 36.0f);
    impl_->root->InvalidateArrange();
    impl_->root->ArrangeInLayout({0.0f, 0.0f, widthDip, heightDip});
    hoverArmed_ = false;
    if (!GetCursorPos(&hoverBaseline_))
    {
        hoverBaseline_ = {};
    }
    float cardLeftDip = 0.0f;
    float cardTopDip = 0.0f;
    if (impl_->card)
    {
        const msimeui::RectF card = impl_->card->GetBounds();
        cardLeftDip = card.x;
        cardTopDip = card.y;
    }
    PlaceAndShow(caret, widthDip, heightDip, cardLeftDip, cardTopDip);
    ::is_global_wnd_cand_shown = true;
}

void CandidatePresenter::Hide()
{
    if (!hwnd_)
    {
        return;
    }
    CloseContextMenu(false);
    ::is_global_wnd_cand_shown = false;
    hoverArmed_ = false;
    if (impl_ && impl_->list)
    {
        impl_->list->SetHoverEnabled(false);
    }
    SetCandidateHostCloaked(true);
    lastHostWidthPx_ = 0;
    lastHostHeightPx_ = 0;
    SetWindowPos(hwnd_, nullptr, 0, Global::INVALID_Y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CandidatePresenter::Present()
{
    if (!bound_ || !hwnd_ || !impl_ || !impl_->window)
    {
        return;
    }
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const UINT width = static_cast<UINT>((std::max)(1L, rc.right));
    const UINT height = static_cast<UINT>((std::max)(1L, rc.bottom));
    if (!impl_->resources.EnsureForComposition(hwnd_))
    {
        return;
    }
    impl_->resources.Resize(width, height);
    ID2D1RenderTarget *target = impl_->resources.GetRenderTarget();
    if (!target)
    {
        return;
    }
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0.0f));
    if (msimeui::Scene *scene = impl_->window->GetScene())
    {
        const float dpi = impl_->window->GetDpi();
        float layoutW = msimeui::PixelsToDips(static_cast<float>(width), dpi);
        float layoutH = msimeui::PixelsToDips(static_cast<float>(height), dpi);
        // Host HWND is larger than the card (shadow padding, 64px buckets, and a
        // grow-only size while typing). Stretching the scene to that HWND fills
        // the rounded card with empty space. Keep layout at the measured DIP size.
        if (!impl_->hostExpandedForMenu && lastLayoutWidthDip_ > 0.0f && lastLayoutHeightDip_ > 0.0f)
        {
            layoutW = lastLayoutWidthDip_;
            layoutH = lastLayoutHeightDip_;
        }
        scene->EnsureLayout({layoutW, layoutH});
        scene->Render(impl_->resources);
    }
    const HRESULT hr = target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        impl_->resources.DiscardTarget();
        return;
    }
    impl_->resources.Present();
}

void CandidatePresenter::ArmHoverIfPointerMoved()
{
    POINT now{};
    if (!GetCursorPos(&now))
    {
        return;
    }
    const int dx = now.x - hoverBaseline_.x;
    const int dy = now.y - hoverBaseline_.y;
    if (dx * dx + dy * dy < 4)
    {
        return;
    }
    hoverArmed_ = true;
    if (impl_ && impl_->list)
    {
        impl_->list->SetHoverEnabled(true);
    }
}

bool CandidatePresenter::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!bound_ || !impl_ || !impl_->window)
    {
        return false;
    }
    switch (message)
    {
    case WM_ERASEBKGND:
        return true;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd_, &ps);
        Present();
        EndPaint(hwnd_, &ps);
        return true;
    }
    case WM_SIZE:
        if (::is_global_wnd_cand_shown)
        {
            impl_->resources.Resize(static_cast<UINT>(LOWORD(lParam)), static_cast<UINT>(HIWORD(lParam)));
            Present();
        }
        return true;
    case WM_MOUSEMOVE:
        ArmHoverIfPointerMoved();
        if (!hoverArmed_ && !(impl_ && impl_->contextMenuOpen))
        {
            return true;
        }
        impl_->window->DispatchImportedMessage(message, wParam, lParam);
        Present();
        return true;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSELEAVE:
        impl_->window->DispatchImportedMessage(message, wParam, lParam);
        Present();
        return true;
    default:
        return false;
    }
}

CandidatePresenter::~CandidatePresenter() = default;
