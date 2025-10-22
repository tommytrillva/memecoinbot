#pragma once

#include <cstddef>

#if __has_include(<imgui.h>)
#  include <imgui.h>
#  define MEMECOINBOT_UI_HAS_IMGUI 1
#else
#  define MEMECOINBOT_UI_HAS_IMGUI 0
#  include <array>
#  include <cstdarg>
#  include <cstdio>
#  include <string>

namespace ImGui {
struct ImVec2 {
    float x;
    float y;
    constexpr ImVec2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
};

struct ImVec4 {
    float x;
    float y;
    float z;
    float w;
    constexpr ImVec4(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f, float _w = 0.0f)
        : x(_x), y(_y), z(_z), w(_w) {}
};

enum ImGuiCol_ {
    ImGuiCol_Text,
    ImGuiCol_TextDisabled,
    ImGuiCol_WindowBg,
    ImGuiCol_ChildBg,
    ImGuiCol_PopupBg,
    ImGuiCol_Border,
    ImGuiCol_BorderShadow,
    ImGuiCol_FrameBg,
    ImGuiCol_FrameBgHovered,
    ImGuiCol_FrameBgActive,
    ImGuiCol_TitleBg,
    ImGuiCol_TitleBgActive,
    ImGuiCol_TitleBgCollapsed,
    ImGuiCol_MenuBarBg,
    ImGuiCol_ScrollbarBg,
    ImGuiCol_ScrollbarGrab,
    ImGuiCol_ScrollbarGrabHovered,
    ImGuiCol_ScrollbarGrabActive,
    ImGuiCol_CheckMark,
    ImGuiCol_SliderGrab,
    ImGuiCol_SliderGrabActive,
    ImGuiCol_Button,
    ImGuiCol_ButtonHovered,
    ImGuiCol_ButtonActive,
    ImGuiCol_Header,
    ImGuiCol_HeaderHovered,
    ImGuiCol_HeaderActive,
    ImGuiCol_Separator,
    ImGuiCol_SeparatorHovered,
    ImGuiCol_SeparatorActive,
    ImGuiCol_Tab,
    ImGuiCol_TabHovered,
    ImGuiCol_TabActive,
    ImGuiCol_TabUnfocused,
    ImGuiCol_TabUnfocusedActive,
    ImGuiCol_COUNT
};

struct ImGuiStyle {
    float WindowRounding = 0.0f;
    float FrameRounding = 0.0f;
    float ScrollbarRounding = 0.0f;
    float GrabRounding = 0.0f;
    float FrameBorderSize = 0.0f;
    float WindowPadding[2] = {8.0f, 8.0f};
    float ItemSpacing[2] = {8.0f, 4.0f};
    std::array<ImVec4, ImGuiCol_COUNT> Colors{};
};

struct ImGuiIO {
    ImVec2 DisplaySize{1280.0f, 720.0f};
    float DeltaTime = 1.0f / 60.0f;
};

enum ImGuiStyleVar_ {
    ImGuiStyleVar_WindowPadding,
    ImGuiStyleVar_WindowRounding,
    ImGuiStyleVar_FramePadding,
    ImGuiStyleVar_FrameRounding,
    ImGuiStyleVar_ItemSpacing,
    ImGuiStyleVar_COUNT
};

inline ImGuiStyle &GetStyle() {
    static ImGuiStyle style;
    return style;
}

inline ImGuiIO &GetIO() {
    static ImGuiIO io;
    return io;
}

inline void StyleColorsDark(ImGuiStyle *dst = nullptr) {
    ImGuiStyle &style = dst ? *dst : GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
}

inline void PushStyleColor(int, ImVec4) {}
inline void PopStyleColor(int = 1) {}
inline void PushStyleVar(ImGuiStyleVar_, float) {}
inline void PushStyleVar(ImGuiStyleVar_, ImVec2) {}
inline void PopStyleVar(int = 1) {}

inline void Separator() {}
inline void SameLine(float = 0.0f, float = -1.0f) {}

inline bool Begin(const char *, bool * = nullptr, int = 0) { return true; }
inline void End() {}
inline ImVec2 GetContentRegionAvail() { return ImVec2(GetIO().DisplaySize.x, GetIO().DisplaySize.y); }
inline bool BeginChild(const char *, ImVec2 = ImVec2(0, 0), bool = false, int = 0) { return true; }
inline void EndChild() {}

inline void TextUnformatted(const char *text) {
    (void)text;
}

inline void Text(const char *fmt, ...) {
    (void)fmt;
}

inline void PlotLines(const char *, const float *, int, int = 0, const char * = nullptr, float = 0.0f,
                      float = 0.0f, ImVec2 = ImVec2(0, 0)) {}

} // namespace ImGui

#endif
