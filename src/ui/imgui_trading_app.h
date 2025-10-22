#pragma once

#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "trading/engine.h"
#include "ui/imgui_helpers.h"

namespace ui {

// TradingImGuiApp orchestrates an ImGui-driven control surface for the
// memecoin trading engine. It does not own the windowing or rendering
// backend; instead it provides the layout and interaction logic that can be
// plugged into any platform integration.
class TradingImGuiApp {
public:
    struct OrderEntryState {
        std::array<char, 64> symbol_buffer{};
        double quantity = 0.0;
        double price = 0.0;
    };

    struct RiskLimitState {
        double max_position = 0.0;
        double max_exposure = 0.0;
    };

    TradingImGuiApp();

    void attachEngine(std::shared_ptr<trading::TradingEngine> engine);

    // Initializes the Dear ImGui context. Safe to call multiple times.
    void initialize();

    // Destroys the ImGui context created during initialize().
    void shutdown();

    // Starts a new ImGui frame. The caller should invoke this once per frame
    // before render().
    void beginFrame();

    // Renders the UI widgets for the trading console.
    void render();

    // Completes the frame after render() has been called. The caller is
    // responsible for handing the generated draw data to a renderer.
    void endFrame();

    // Pushes a message to the rolling log panel rendered by the UI.
    void enqueueLogMessage(std::string message);

    // Exposes the mutable state for unit testing or external integrations.
    OrderEntryState& orderEntryState();
    RiskLimitState& riskLimitState();

    void setShowDemoWindow(bool show_demo);
    bool showDemoWindow() const;

private:
    void renderStatusSection();
    void renderOrderEntrySection();
    void renderRiskSection();
    void renderLogSection();

    std::shared_ptr<trading::TradingEngine> engine_;
    OrderEntryState order_entry_{};
    RiskLimitState risk_limits_{};

    bool initialized_ = false;
    bool show_demo_window_ = false;

    std::deque<std::string> log_messages_;
    std::size_t max_log_messages_ = 200;

    mutable std::mutex log_mutex_;
};

}  // namespace ui

