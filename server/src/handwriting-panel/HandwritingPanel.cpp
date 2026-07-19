#include "HandwritingPanel.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <thread>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.Input.Inking.h>

namespace msimeui
{
namespace
{
constexpr size_t kInvalid = static_cast<size_t>(-1);

bool Contains(const RectF &r, const PointF &p)
{
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

D2D1_RECT_F D2DRect(const RectF &r) { return D2D1::RectF(r.x, r.y, r.x + r.width, r.y + r.height); }

void Fill(DeviceResources &resources, const RectF &rect, D2D1_COLOR_F color, float radius = 0.0f)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush) return;
    if (radius > 0.0f)
        target->FillRoundedRectangle(D2D1::RoundedRect(D2DRect(rect), radius, radius), brush);
    else
        target->FillRectangle(D2DRect(rect), brush);
}

void Outline(DeviceResources &resources, const RectF &rect, D2D1_COLOR_F color, float width = 1.0f, float radius = 0.0f)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush) return;
    if (radius > 0.0f)
        target->DrawRoundedRectangle(D2D1::RoundedRect(D2DRect(rect), radius, radius), brush, width);
    else
        target->DrawRectangle(D2DRect(rect), brush, width);
}

void Text(DeviceResources &resources, const std::wstring &text, const RectF &rect, float size, D2D1_COLOR_F color,
          DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL)
{
    auto *target = resources.GetRenderTarget();
    auto *format = resources.GetTextFormat(L"Noto Sans SC", size, weight, alignment,
                                            DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *brush = resources.GetSolidColorBrush(color);
    if (target && format && brush)
        target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, D2DRect(rect), brush,
                          D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void DrawCloseIcon(DeviceResources &resources, const RectF &rect, D2D1_COLOR_F color)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush) return;
    const float cx = rect.x + rect.width * 0.5f;
    const float cy = rect.y + rect.height * 0.5f;
    const float half = std::min(rect.width, rect.height) * 0.19f;
    target->DrawLine({cx - half, cy - half}, {cx + half, cy + half}, brush, 1.5f);
    target->DrawLine({cx + half, cy - half}, {cx - half, cy + half}, brush, 1.5f);
}

void DrawInkStroke(DeviceResources &resources, const std::vector<PointF> &points, ID2D1Brush *brush)
{
    auto *target = resources.GetRenderTarget();
    if (!target || !brush || points.size() < 2) return;

    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    target->GetFactory(&factory);
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (!factory || FAILED(factory->CreatePathGeometry(&geometry)) || FAILED(geometry->Open(&sink))) return;

    sink->BeginFigure({points.front().x, points.front().y}, D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 1; i < points.size(); ++i) sink->AddLine({points[i].x, points[i].y});
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    if (FAILED(sink->Close())) return;

    D2D1_STROKE_STYLE_PROPERTIES properties = {};
    properties.startCap = D2D1_CAP_STYLE_ROUND;
    properties.endCap = D2D1_CAP_STYLE_ROUND;
    properties.dashCap = D2D1_CAP_STYLE_ROUND;
    properties.lineJoin = D2D1_LINE_JOIN_ROUND;
    properties.miterLimit = 10.0f;
    properties.dashStyle = D2D1_DASH_STYLE_SOLID;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> strokeStyle;
    if (FAILED(factory->CreateStrokeStyle(properties, nullptr, 0, &strokeStyle))) return;
    target->DrawGeometry(geometry.Get(), brush, 4.0f, strokeStyle.Get());
}

void AddUniqueCandidate(std::vector<std::wstring> &candidates, const winrt::hstring &value)
{
    std::wstring text(value.c_str(), value.size());
    if (!text.empty() && candidates.size() < 12 &&
        std::find(candidates.begin(), candidates.end(), text) == candidates.end())
        candidates.push_back(std::move(text));
}

bool LooksLikeChineseRecognizer(const winrt::hstring &name)
{
    std::wstring value(name.c_str(), name.size());
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) { return std::towlower(ch); });
    return value.find(L"\u4e2d\u6587") != std::wstring::npos || value.find(L"\u7b80\u4f53") != std::wstring::npos ||
           value.find(L"chinese") != std::wstring::npos || value.find(L"zh-cn") != std::wstring::npos;
}

bool ContainsCjk(const std::wstring &text)
{
    return std::any_of(text.begin(), text.end(), [](wchar_t ch) {
        return (ch >= 0x3400 && ch <= 0x4DBF) || (ch >= 0x4E00 && ch <= 0x9FFF) ||
               (ch >= 0xF900 && ch <= 0xFAFF);
    });
}
} // namespace

SizeF HandwritingPanel::Measure(const SizeF &availableSize) { return availableSize; }

void HandwritingPanel::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    RebuildLayout();
}

void HandwritingPanel::RebuildLayout()
{
    const float pad = 24.0f;
    headerRect_ = {bounds_.x, bounds_.y, bounds_.width, 38.0f};
    closeRect_ = {bounds_.x + bounds_.width - 30.0f, bounds_.y + 7.0f, 24.0f, 24.0f};
    const float top = bounds_.y + 58.0f;
    const float contentWidth = std::max(1.0f, bounds_.width - pad * 2.0f);
    const float leftWidth = contentWidth * 0.43f;
    canvasRect_ = {bounds_.x + pad, top, leftWidth, leftWidth};
    const float bottom = canvasRect_.y + canvasRect_.height;
    resultsRect_ = {canvasRect_.x + canvasRect_.width + 28.0f, top,
                    std::max(1.0f, contentWidth - leftWidth - 28.0f),
                    std::max(1.0f, bounds_.y + bounds_.height - pad - top)};
    undoRect_ = {canvasRect_.x, bottom + 18.0f, 112.0f, 42.0f};
    clearRect_ = {undoRect_.x + undoRect_.width + 12.0f, undoRect_.y, 112.0f, 42.0f};
    candidateRects_.clear();
    const float gap = 8.0f;
    const float cellWidth = (resultsRect_.width - gap * 3.0f) / 4.0f;
    const float gridTop = resultsRect_.y + 42.0f;
    for (size_t i = 0; i < 12; ++i)
        candidateRects_.push_back({resultsRect_.x + static_cast<float>(i % 4) * (cellWidth + gap),
                                   gridTop + static_cast<float>(i / 4) * (cellWidth + gap),
                                   cellWidth, cellWidth});
}

void HandwritingPanel::Render(DeviceResources &resources)
{
    Fill(resources, bounds_, D2D1::ColorF(0x202027));
    Fill(resources, headerRect_, D2D1::ColorF(0x202027));
    Text(resources, L"\u6c34\u6749\u624b\u5199\u8bc6\u522b\u677f",
         {headerRect_.x + 14.0f, headerRect_.y, 220.0f, headerRect_.height}, 14.0f, D2D1::ColorF(0xF5F5F7),
         DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_NORMAL);
    if (closeHovered_ || closePressed_)
        Fill(resources, closeRect_, closePressed_ ? D2D1::ColorF(0x4A4B55) : D2D1::ColorF(0x363740), 5.0f);
    DrawCloseIcon(resources, closeRect_, D2D1::ColorF(0xF4F4F7));

    Fill(resources, canvasRect_, D2D1::ColorF(0x25262D), 8.0f);
    Outline(resources, canvasRect_, D2D1::ColorF(0x45454F), 1.0f, 8.0f);
    auto *target = resources.GetRenderTarget();
    auto *inkBrush = resources.GetSolidColorBrush(D2D1::ColorF(0xF4F4F7));
    if (target && inkBrush)
    {
        for (const auto &stroke : strokes_) DrawInkStroke(resources, stroke, inkBrush);
    }
    if (strokes_.empty())
        Text(resources, L"\u8bf7\u5728\u8fd9\u91cc\u4e66\u5199", canvasRect_, 18.0f, D2D1::ColorF(0x74757E));

    Text(resources, L"\u8bc6\u522b\u7ed3\u679c", {resultsRect_.x, resultsRect_.y, resultsRect_.width, 30.0f}, 18.0f,
         D2D1::ColorF(0xF5F5F7), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    for (size_t i = 0; i < candidateRects_.size(); ++i)
    {
        const bool hovered = i == hoveredCandidate_;
        Fill(resources, candidateRects_[i], hovered ? D2D1::ColorF(0x3B3240) : D2D1::ColorF(0x292A31), 6.0f);
        Outline(resources, candidateRects_[i], hovered ? D2D1::ColorF(0xD88BDE) : D2D1::ColorF(0x45454F),
                hovered ? 2.0f : 1.0f, 5.0f);
        if (i < candidates_.size())
        {
            // The window is sized in physical pixels while Direct2D lays out in DIPs. Keep glyphs
            // proportional to the actual cell so high-DPI displays cannot clip the candidates.
            const float lengthScale = candidates_[i].size() > 1 ? static_cast<float>(candidates_[i].size()) : 1.0f;
            const float fontSize = std::clamp(
                std::min(candidateRects_[i].height * 0.52f, (candidateRects_[i].width - 10.0f) / lengthScale),
                13.0f, 34.0f);
            const RectF textRect = {candidateRects_[i].x + 3.0f, candidateRects_[i].y + 2.0f,
                                    candidateRects_[i].width - 6.0f, candidateRects_[i].height - 4.0f};
            Text(resources, candidates_[i], textRect, fontSize, D2D1::ColorF(0xF5F5F7));
        }
    }
    Text(resources, hint_, {resultsRect_.x, resultsRect_.y + resultsRect_.height - 34.0f, resultsRect_.width, 28.0f},
         14.0f, D2D1::ColorF(0xB8B8C0), DWRITE_TEXT_ALIGNMENT_LEADING);

    Fill(resources, undoRect_, D2D1::ColorF(0x292A31), 6.0f);
    Fill(resources, clearRect_, D2D1::ColorF(0x292A31), 6.0f);
    Outline(resources, undoRect_, D2D1::ColorF(0x45454F), 1.0f, 6.0f);
    Outline(resources, clearRect_, D2D1::ColorF(0x45454F), 1.0f, 6.0f);
    Text(resources, L"\u21b6  \u64a4\u9500", undoRect_, 15.0f, D2D1::ColorF(0xF5F5F7));
    Text(resources, L"\u2715  \u91cd\u5199", clearRect_, 15.0f, D2D1::ColorF(0xF5F5F7));
}

bool HandwritingPanel::HitTest(const PointF &point) const { return Contains(bounds_, point); }

size_t HandwritingPanel::HitCandidate(const PointF &point) const
{
    for (size_t i = 0; i < candidateRects_.size() && i < candidates_.size(); ++i)
        if (Contains(candidateRects_[i], point)) return i;
    return kInvalid;
}

bool HandwritingPanel::OnMouseDown(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF p = window_->ClientPixelsToDips(point);
    closePressed_ = Contains(closeRect_, p);
    if (closePressed_)
    {
        InvalidateVisual();
        return true;
    }
    pressedCandidate_ = HitCandidate(p);
    if (Contains(canvasRect_, p))
    {
        drawing_ = true;
        strokes_.push_back({p});
    }
    InvalidateVisual();
    return true;
}

bool HandwritingPanel::OnMouseMove(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF p = window_->ClientPixelsToDips(point);
    const bool closeHover = Contains(closeRect_, p);
    const size_t hover = HitCandidate(p);
    if (hover != hoveredCandidate_ || closeHover != closeHovered_)
    {
        hoveredCandidate_ = hover;
        closeHovered_ = closeHover;
        InvalidateVisual();
    }
    if (drawing_ && !strokes_.empty())
    {
        PointF clipped = {std::clamp(p.x, canvasRect_.x, canvasRect_.x + canvasRect_.width),
                          std::clamp(p.y, canvasRect_.y, canvasRect_.y + canvasRect_.height)};
        const PointF last = strokes_.back().back();
        if (std::abs(clipped.x - last.x) + std::abs(clipped.y - last.y) >= 0.5f)
        {
            strokes_.back().push_back(clipped);
            InvalidateVisual();
        }
    }
    return true;
}

bool HandwritingPanel::OnMouseUp(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF p = window_->ClientPixelsToDips(point);
    const bool close = closePressed_ && Contains(closeRect_, p);
    closePressed_ = false;
    if (close)
    {
        PostMessageW(window_->GetHandle(), WM_CLOSE, 0, 0);
    }
    else if (drawing_)
    {
        drawing_ = false;
        if (strokes_.back().size() < 2) strokes_.pop_back();
        Recognize();
    }
    else if (pressedCandidate_ != kInvalid && pressedCandidate_ == HitCandidate(p)) CopyCandidate(pressedCandidate_);
    else if (Contains(undoRect_, p))
    {
        if (!strokes_.empty()) strokes_.pop_back();
        Recognize();
    }
    else if (Contains(clearRect_, p)) Clear();
    pressedCandidate_ = kInvalid;
    InvalidateVisual();
    return true;
}

void HandwritingPanel::OnMouseLeave()
{
    hoveredCandidate_ = kInvalid;
    closeHovered_ = false;
    InvalidateVisual();
}

void HandwritingPanel::Recognize()
{
    candidates_.clear();
    if (strokes_.empty())
    {
        hint_ = L"\u5728\u5de6\u4fa7\u4e66\u5199\uff0c\u677e\u5f00\u9f20\u6807\u540e\u81ea\u52a8\u8bc6\u522b";
        return;
    }
    std::wstring error;
    const auto inputStrokes = strokes_;
    std::thread worker([&]() {
        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            using namespace winrt::Windows::Foundation;
            using namespace winrt::Windows::Foundation::Numerics;
            using namespace winrt::Windows::UI::Input::Inking;

            InkRecognizerContainer recognizerContainer;
            bool hasChineseRecognizer = false;
            for (const auto &recognizer : recognizerContainer.GetRecognizers())
            {
                if (LooksLikeChineseRecognizer(recognizer.Name()))
                {
                    recognizerContainer.SetDefaultRecognizer(recognizer);
                    hasChineseRecognizer = true;
                    break;
                }
            }
            if (!hasChineseRecognizer)
            {
                error = L"\u672a\u627e\u5230\u7b80\u4f53\u4e2d\u6587\u624b\u5199\u8bc6\u522b\u5668";
                return;
            }

            InkStrokeContainer strokeContainer;
            InkStrokeBuilder builder;
            for (const auto &sourceStroke : inputStrokes)
            {
                auto points = winrt::single_threaded_vector<InkPoint>();
                for (const auto &point : sourceStroke)
                    points.Append(InkPoint({point.x - canvasRect_.x, point.y - canvasRect_.y}, 0.5f));
                const float3x2 identity = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
                InkStroke stroke = builder.CreateStrokeFromInkPoints(points, identity);
                strokeContainer.AddStroke(stroke);
            }

            std::vector<std::wstring> chineseCandidates;
            std::vector<std::wstring> otherCandidates;
            const auto results = recognizerContainer.RecognizeAsync(strokeContainer, InkRecognitionTarget::All).get();
            for (const auto &result : results)
            {
                for (const auto &candidate : result.GetTextCandidates())
                {
                    std::wstring value(candidate.c_str(), candidate.size());
                    if (ContainsCjk(value))
                        AddUniqueCandidate(chineseCandidates, candidate);
                    else
                        AddUniqueCandidate(otherCandidates, candidate);
                }
            }
            for (const auto &candidate : chineseCandidates)
                AddUniqueCandidate(candidates_, winrt::hstring(candidate));
            for (const auto &candidate : otherCandidates)
                AddUniqueCandidate(candidates_, winrt::hstring(candidate));
        }
        catch (const winrt::hresult_error &exception)
        {
            error.assign(exception.message().c_str(), exception.message().size());
        }
    });
    worker.join();

    if (!error.empty())
        hint_ = L"Windows Ink \u8bc6\u522b\u5931\u8d25\uff1a" + error;
    else
        hint_ = candidates_.empty() ? L"\u672a\u8bc6\u522b\u5230\u5185\u5bb9\uff0c\u8bf7\u786e\u8ba4\u5df2\u5b89\u88c5\u4e2d\u6587\u624b\u5199\u5305"
                                    : L"\u70b9\u51fb\u5019\u9009\u7ed3\u679c\u5373\u53ef\u590d\u5236";
}

void HandwritingPanel::CopyCandidate(size_t index)
{
    if (index >= candidates_.size() || !window_ || !OpenClipboard(window_->GetHandle())) return;
    EmptyClipboard();
    const SIZE_T bytes = (candidates_[index].size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory)
    {
        void *destination = GlobalLock(memory);
        if (destination)
        {
            memcpy(destination, candidates_[index].c_str(), bytes);
            GlobalUnlock(memory);
            if (SetClipboardData(CF_UNICODETEXT, memory)) memory = nullptr;
        }
        if (memory) GlobalFree(memory);
    }
    CloseClipboard();
    hint_ = L"\u5df2\u590d\u5236\uff1a" + candidates_[index];
}

void HandwritingPanel::Clear()
{
    strokes_.clear();
    candidates_.clear();
    hint_ = L"\u5df2\u6e05\u7a7a\uff0c\u8bf7\u91cd\u65b0\u4e66\u5199";
}

HCURSOR HandwritingPanel::GetCursor() const { return LoadCursor(nullptr, drawing_ ? IDC_CROSS : IDC_HAND); }
} // namespace msimeui

