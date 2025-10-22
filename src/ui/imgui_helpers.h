#pragma once

#include <cstddef>

#if __has_include(<imgui.h>)
#include <imgui.h>
#else
struct ImVec2 {
    float x;
    float y;
    ImVec2(float value_x = 0.0f, float value_y = 0.0f) : x(value_x), y(value_y) {}
};

namespace ImGui {
struct IO {
    bool WantCaptureMouse = false;
    bool WantCaptureKeyboard = false;
    float DeltaTime = 1.0f;
};

inline void* CreateContext(...) { return nullptr; }
inline void DestroyContext(...) {}
inline IO& GetIO() {
    static IO io;
    return io;
}
inline void NewFrame() {}
inline void EndFrame() {}
inline void Render() {}
inline bool Begin(const char*, bool* = nullptr, int = 0) { return true; }
inline void End() {}
inline void Text(const char*, ...) {}
inline void TextUnformatted(const char*) {}
inline void Separator() {}
inline bool Button(const char*, const ImVec2& = ImVec2()) { return false; }
inline bool Checkbox(const char*, bool*) { return false; }
inline bool InputText(const char*, char*, std::size_t, int = 0, void* = nullptr, void* = nullptr) { return false; }
inline bool InputDouble(const char*, double*, double = 0.0, double = 0.0, const char* = "%.6f", int = 0) { return false; }
inline bool CollapsingHeader(const char*, int = 0) { return true; }
inline void SameLine(float = 0.0f, float = -1.0f) {}
inline void SetNextItemWidth(float) {}
inline bool BeginChild(const char*, const ImVec2& = ImVec2(), bool = false, int = 0) { return true; }
inline void EndChild() {}
inline void ShowDemoWindow(bool*) {}
inline void Dummy(const ImVec2&) {}
}  // namespace ImGui
#endif

