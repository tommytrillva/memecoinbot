#include "ui/layout.h"

#include <algorithm>

namespace ui {

DashboardLayout::DashboardLayout(float chart_height_ratio, float min_bottom_height, float min_column_width,
                                 float gutter)
    : m_chart_height_ratio(chart_height_ratio),
      m_min_bottom_height(min_bottom_height),
      m_min_column_width(min_column_width),
      m_gutter(gutter) {}

DashboardLayoutState DashboardLayout::compute(const ImGui::ImVec2 &available_region) const {
    DashboardLayoutState state{};
    if (available_region.x <= 0.0f || available_region.y <= 0.0f) {
        return state;
    }

    float desired_chart_height = available_region.y * m_chart_height_ratio;
    float bottom_height = available_region.y - desired_chart_height;
    if (bottom_height < m_min_bottom_height) {
        bottom_height = std::min(m_min_bottom_height, available_region.y);
        desired_chart_height = std::max(0.0f, available_region.y - bottom_height);
    }

    state.price_chart_size = ImGui::ImVec2(available_region.x, desired_chart_height);

    const bool stack_vertical = available_region.x < (2.0f * m_min_column_width + m_gutter);
    state.stack_trades_below_positions = stack_vertical;

    if (stack_vertical) {
        float available_height = std::max(0.0f, bottom_height - m_gutter);
        float pane_height = available_height * 0.5f;
        state.positions_size = ImGui::ImVec2(available_region.x, pane_height);
        state.trades_size = ImGui::ImVec2(available_region.x, pane_height);
    } else {
        float column_width = (available_region.x - m_gutter) * 0.5f;
        column_width = std::max(column_width, m_min_column_width);
        state.positions_size = ImGui::ImVec2(column_width, bottom_height);
        state.trades_size = ImGui::ImVec2(column_width, bottom_height);
    }

    return state;
}

} // namespace ui
