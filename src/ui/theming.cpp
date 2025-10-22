#include "ui/theming.h"

namespace ui {

Theme create_neon_dark_theme() {
    Theme theme;
    theme.palette.background = ImGui::ImVec4(0.04f, 0.05f, 0.10f, 1.0f);
    theme.palette.surface = ImGui::ImVec4(0.08f, 0.10f, 0.16f, 1.0f);
    theme.palette.surface_alt = ImGui::ImVec4(0.12f, 0.14f, 0.22f, 1.0f);
    theme.palette.accent_primary = ImGui::ImVec4(0.00f, 0.92f, 0.72f, 1.0f);
    theme.palette.accent_secondary = ImGui::ImVec4(0.52f, 0.20f, 0.88f, 1.0f);
    theme.palette.text_primary = ImGui::ImVec4(0.88f, 0.92f, 1.00f, 1.0f);
    theme.palette.text_muted = ImGui::ImVec4(0.50f, 0.58f, 0.70f, 1.0f);
    theme.palette.positive = ImGui::ImVec4(0.00f, 0.80f, 0.46f, 1.0f);
    theme.palette.negative = ImGui::ImVec4(0.96f, 0.24f, 0.36f, 1.0f);

    theme.metrics.window_rounding = 12.0f;
    theme.metrics.frame_rounding = 8.0f;
    theme.metrics.frame_padding = 12.0f;
    theme.metrics.item_spacing = 10.0f;
    theme.metrics.border_thickness = 1.5f;

    return theme;
}

void Theme::apply() const {
    ImGui::StyleColorsDark();
    ImGui::ImGuiStyle &style = ImGui::GetStyle();

    style.WindowRounding = metrics.window_rounding;
    style.FrameRounding = metrics.frame_rounding;
    style.FrameBorderSize = metrics.border_thickness;
    style.WindowPadding[0] = metrics.frame_padding;
    style.WindowPadding[1] = metrics.frame_padding;
    style.ItemSpacing[0] = metrics.item_spacing;
    style.ItemSpacing[1] = metrics.item_spacing * 0.6f;

    auto &colors = style.Colors;
    colors[ImGui::ImGuiCol_WindowBg] = palette.background;
    colors[ImGui::ImGuiCol_ChildBg] = palette.surface;
    colors[ImGui::ImGuiCol_PopupBg] = palette.surface_alt;
    colors[ImGui::ImGuiCol_Border] = palette.accent_primary;
    colors[ImGui::ImGuiCol_Text] = palette.text_primary;
    colors[ImGui::ImGuiCol_TextDisabled] = palette.text_muted;
    colors[ImGui::ImGuiCol_Header] = palette.surface_alt;
    colors[ImGui::ImGuiCol_HeaderHovered] = palette.accent_secondary;
    colors[ImGui::ImGuiCol_HeaderActive] = palette.accent_primary;
    colors[ImGui::ImGuiCol_Button] = palette.accent_secondary;
    colors[ImGui::ImGuiCol_ButtonHovered] = palette.accent_primary;
    colors[ImGui::ImGuiCol_ButtonActive] = palette.accent_primary;
    colors[ImGui::ImGuiCol_FrameBg] = palette.surface;
    colors[ImGui::ImGuiCol_FrameBgHovered] = palette.surface_alt;
    colors[ImGui::ImGuiCol_FrameBgActive] = palette.accent_secondary;
    colors[ImGui::ImGuiCol_Separator] = palette.accent_primary;
    colors[ImGui::ImGuiCol_Tab] = palette.surface;
    colors[ImGui::ImGuiCol_TabHovered] = palette.accent_secondary;
    colors[ImGui::ImGuiCol_TabActive] = palette.accent_primary;
    colors[ImGui::ImGuiCol_TabUnfocused] = palette.surface_alt;
    colors[ImGui::ImGuiCol_TabUnfocusedActive] = palette.accent_secondary;
    colors[ImGui::ImGuiCol_SliderGrab] = palette.accent_primary;
    colors[ImGui::ImGuiCol_SliderGrabActive] = palette.accent_secondary;
    colors[ImGui::ImGuiCol_CheckMark] = palette.accent_primary;
}

} // namespace ui
