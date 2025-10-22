#pragma once

#include "ui/imgui_compat.h"

namespace ui {

struct DashboardLayoutState {
    ImGui::ImVec2 price_chart_size{0.0f, 0.0f};
    ImGui::ImVec2 positions_size{0.0f, 0.0f};
    ImGui::ImVec2 trades_size{0.0f, 0.0f};
    bool stack_trades_below_positions = false;
};

class DashboardLayout {
  public:
    DashboardLayout(float chart_height_ratio = 0.6f, float min_bottom_height = 220.0f,
                    float min_column_width = 320.0f, float gutter = 12.0f);

    DashboardLayoutState compute(const ImGui::ImVec2 &available_region) const;
    float gutter() const noexcept { return m_gutter; }

  private:
    float m_chart_height_ratio;
    float m_min_bottom_height;
    float m_min_column_width;
    float m_gutter;
};

} // namespace ui
