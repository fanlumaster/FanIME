#pragma once

#include "msimeui/Layout.h"

#include <string>
#include <vector>

namespace msimeui
{
class HandwritingPanel final : public Visual
{
  public:
    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &resources) override;
    bool HitTest(const PointF &point) const override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    void OnMouseLeave() override;
    HCURSOR GetCursor() const override;

  private:
    void RebuildLayout();
    void Recognize();
    void CopyCandidate(size_t index);
    size_t HitCandidate(const PointF &point) const;
    void Clear();

    std::vector<std::vector<PointF>> strokes_;
    std::vector<std::wstring> candidates_;
    std::wstring hint_ = L"\u5728\u5de6\u4fa7\u4e66\u5199\uff0c\u677e\u5f00\u9f20\u6807\u540e\u81ea\u52a8\u8bc6\u522b";
    RectF headerRect_ = {};
    RectF canvasRect_ = {};
    RectF resultsRect_ = {};
    RectF undoRect_ = {};
    RectF clearRect_ = {};
    RectF closeRect_ = {};
    std::vector<RectF> candidateRects_;
    bool drawing_ = false;
    bool closeHovered_ = false;
    bool closePressed_ = false;
    size_t hoveredCandidate_ = static_cast<size_t>(-1);
    size_t pressedCandidate_ = static_cast<size_t>(-1);
};
} // namespace msimeui

