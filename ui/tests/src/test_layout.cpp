#include "tests/includes/test_framework.h"

#include "msimeui/Layout.h"

#include <cmath>
#include <memory>

using namespace msimeui;

namespace
{
// A leaf visual with a fixed desired size. It reports its size verbatim rather
// than clamping to the available space, so the tests can observe the clamping
// that Visual::MeasureInLayout is responsible for.
class FixedVisual : public Visual
{
  public:
    explicit FixedVisual(SizeF size) : size_(size)
    {
    }

    SizeF Measure(const SizeF &) override
    {
        ++measureCount;
        return size_;
    }

    void Arrange(const RectF &finalRect) override
    {
        bounds_ = finalRect;
    }

    void Render(DeviceResources &) override
    {
    }

    int measureCount = 0;

  private:
    SizeF size_;
};

std::shared_ptr<FixedVisual> MakeFixed(float width, float height)
{
    return std::make_shared<FixedVisual>(SizeF{width, height});
}
} // namespace

TEST_CASE(margin_deflates_the_available_size_and_inflates_the_reported_size)
{
    auto visual = MakeFixed(100.0f, 50.0f);
    visual->SetMargin(10.0f);

    const SizeF outer = visual->MeasureInLayout({200.0f, 200.0f});

    REQUIRE_NEAR(outer.width, 120.0f);
    REQUIRE_NEAR(outer.height, 70.0f);
}

TEST_CASE(explicit_size_overrides_the_measured_size)
{
    auto visual = MakeFixed(100.0f, 50.0f);
    visual->SetWidth(80.0f);
    visual->SetHeight(20.0f);

    const SizeF outer = visual->MeasureInLayout({200.0f, 200.0f});

    REQUIRE_NEAR(outer.width, 80.0f);
    REQUIRE_NEAR(outer.height, 20.0f);
}

TEST_CASE(min_and_max_constraints_clamp_the_desired_size)
{
    auto visual = MakeFixed(100.0f, 50.0f);
    visual->SetMinWidth(150.0f);
    visual->SetMaxHeight(30.0f);

    const SizeF outer = visual->MeasureInLayout({400.0f, 400.0f});

    REQUIRE_NEAR(outer.width, 150.0f);
    REQUIRE_NEAR(outer.height, 30.0f);
}

TEST_CASE(the_desired_size_never_exceeds_the_available_space)
{
    auto visual = MakeFixed(500.0f, 500.0f);

    const SizeF outer = visual->MeasureInLayout({200.0f, 100.0f});

    REQUIRE_NEAR(outer.width, 200.0f);
    REQUIRE_NEAR(outer.height, 100.0f);
}

TEST_CASE(measuring_twice_with_the_same_available_size_reuses_the_cache)
{
    auto visual = MakeFixed(100.0f, 50.0f);

    visual->MeasureInLayout({200.0f, 200.0f});
    visual->MeasureInLayout({200.0f, 200.0f});
    REQUIRE(visual->measureCount == 1);

    // A different available size has to invalidate the cached measurement.
    visual->MeasureInLayout({300.0f, 200.0f});
    REQUIRE(visual->measureCount == 2);
}

TEST_CASE(stretch_alignment_fills_the_arranged_slot)
{
    auto visual = MakeFixed(40.0f, 20.0f);
    visual->SetHorizontalAlignment(HorizontalAlignment::Stretch);
    visual->SetVerticalAlignment(VerticalAlignment::Stretch);

    visual->MeasureInLayout({200.0f, 100.0f});
    visual->ArrangeInLayout({0.0f, 0.0f, 200.0f, 100.0f});

    const RectF &bounds = visual->GetBounds();
    REQUIRE_NEAR(bounds.width, 200.0f);
    REQUIRE_NEAR(bounds.height, 100.0f);
}

TEST_CASE(center_alignment_positions_the_visual_inside_the_slot)
{
    auto visual = MakeFixed(40.0f, 20.0f);
    visual->SetHorizontalAlignment(HorizontalAlignment::Center);
    visual->SetVerticalAlignment(VerticalAlignment::Center);

    visual->MeasureInLayout({200.0f, 100.0f});
    visual->ArrangeInLayout({0.0f, 0.0f, 200.0f, 100.0f});

    const RectF &bounds = visual->GetBounds();
    REQUIRE_NEAR(bounds.x, 80.0f);
    REQUIRE_NEAR(bounds.y, 40.0f);
    REQUIRE_NEAR(bounds.width, 40.0f);
    REQUIRE_NEAR(bounds.height, 20.0f);
}

TEST_CASE(trailing_alignment_pushes_the_visual_to_the_far_edge)
{
    auto visual = MakeFixed(40.0f, 20.0f);
    visual->SetHorizontalAlignment(HorizontalAlignment::Trailing);
    visual->SetVerticalAlignment(VerticalAlignment::Trailing);

    visual->MeasureInLayout({200.0f, 100.0f});
    visual->ArrangeInLayout({0.0f, 0.0f, 200.0f, 100.0f});

    const RectF &bounds = visual->GetBounds();
    REQUIRE_NEAR(bounds.x, 160.0f);
    REQUIRE_NEAR(bounds.y, 80.0f);
}

TEST_CASE(margin_offsets_the_arranged_bounds)
{
    auto visual = MakeFixed(40.0f, 20.0f);
    visual->SetMargin(Thickness{10.0f, 5.0f, 0.0f, 0.0f});
    visual->SetHorizontalAlignment(HorizontalAlignment::Leading);
    visual->SetVerticalAlignment(VerticalAlignment::Leading);

    visual->MeasureInLayout({200.0f, 100.0f});
    visual->ArrangeInLayout({0.0f, 0.0f, 200.0f, 100.0f});

    const RectF &bounds = visual->GetBounds();
    REQUIRE_NEAR(bounds.x, 10.0f);
    REQUIRE_NEAR(bounds.y, 5.0f);
}

TEST_CASE(stack_panel_reports_the_children_height_plus_spacing)
{
    auto panel = std::make_shared<StackPanel>(8.0f);
    panel->AddChild(MakeFixed(60.0f, 20.0f));
    panel->AddChild(MakeFixed(40.0f, 30.0f));

    const SizeF outer = panel->MeasureInLayout({200.0f, 200.0f});

    // Widest child, and both heights with one gap between them.
    REQUIRE_NEAR(outer.width, 60.0f);
    REQUIRE_NEAR(outer.height, 58.0f);
}

TEST_CASE(stack_panel_advances_each_child_by_its_height_and_the_spacing)
{
    auto first = MakeFixed(60.0f, 20.0f);
    auto second = MakeFixed(40.0f, 30.0f);
    auto panel = std::make_shared<StackPanel>(8.0f);
    panel->AddChild(first);
    panel->AddChild(second);

    panel->MeasureInLayout({200.0f, 200.0f});
    panel->ArrangeInLayout({0.0f, 0.0f, 200.0f, 100.0f});

    REQUIRE_NEAR(first->GetBounds().y, 0.0f);
    REQUIRE_NEAR(first->GetBounds().height, 20.0f);
    REQUIRE_NEAR(second->GetBounds().y, 28.0f);
    REQUIRE_NEAR(second->GetBounds().height, 30.0f);
}

TEST_CASE(stack_panel_padding_insets_its_children)
{
    auto child = MakeFixed(40.0f, 20.0f);
    auto panel = std::make_shared<StackPanel>();
    panel->SetPadding(12.0f);
    panel->AddChild(child);

    panel->MeasureInLayout({200.0f, 200.0f});
    panel->ArrangeInLayout({0.0f, 0.0f, 200.0f, 100.0f});

    REQUIRE_NEAR(child->GetBounds().x, 12.0f);
    REQUIRE_NEAR(child->GetBounds().y, 12.0f);
}

TEST_CASE(horizontal_stack_panel_advances_along_x)
{
    auto first = MakeFixed(60.0f, 20.0f);
    auto second = MakeFixed(40.0f, 30.0f);
    auto panel = std::make_shared<HorizontalStackPanel>(6.0f);
    panel->AddChild(first);
    panel->AddChild(second);

    const SizeF outer = panel->MeasureInLayout({300.0f, 200.0f});
    panel->ArrangeInLayout({0.0f, 0.0f, 300.0f, 100.0f});

    REQUIRE_NEAR(outer.width, 106.0f);
    REQUIRE_NEAR(first->GetBounds().x, 0.0f);
    REQUIRE_NEAR(second->GetBounds().x, 66.0f);
}
