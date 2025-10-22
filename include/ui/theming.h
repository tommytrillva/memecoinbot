#pragma once

#include "ui/imgui_compat.h"

namespace ui {

struct ThemePalette {
    ImGui::ImVec4 background;
    ImGui::ImVec4 surface;
    ImGui::ImVec4 surface_alt;
    ImGui::ImVec4 accent_primary;
    ImGui::ImVec4 accent_secondary;
    ImGui::ImVec4 text_primary;
    ImGui::ImVec4 text_muted;
    ImGui::ImVec4 positive;
    ImGui::ImVec4 negative;
};

struct ThemeMetrics {
    float window_rounding = 8.0f;
    float frame_rounding = 6.0f;
    float frame_padding = 10.0f;
    float item_spacing = 8.0f;
    float border_thickness = 1.0f;
};

struct Theme {
    ThemePalette palette;
    ThemeMetrics metrics;

    void apply() const;
};

Theme create_neon_dark_theme();

} // namespace ui
