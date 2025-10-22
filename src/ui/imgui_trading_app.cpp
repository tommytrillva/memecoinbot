#include "ui/imgui_trading_app.h"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace ui {
namespace {
constexpr char kDefaultSymbol[] = "BTC-USD";
constexpr double kDefaultMaxPosition = 25.0;
constexpr double kDefaultMaxExposure = 125.0;
}  // namespace

TradingImGuiApp::TradingImGuiApp() {
    std::fill(order_entry_.symbol_buffer.begin(), order_entry_.symbol_buffer.end(), '\0');
    const auto default_length = std::min(order_entry_.symbol_buffer.size() - 1, sizeof(kDefaultSymbol));
    std::copy_n(kDefaultSymbol, default_length, order_entry_.symbol_buffer.begin());

    risk_limits_.max_position = kDefaultMaxPosition;
    risk_limits_.max_exposure = kDefaultMaxExposure;
}

void TradingImGuiApp::attachEngine(std::shared_ptr<trading::TradingEngine> engine) {
    engine_ = std::move(engine);
}

void TradingImGuiApp::initialize() {
    if (initialized_) {
        return;
    }
    ImGui::CreateContext();
    initialized_ = true;
}

void TradingImGuiApp::shutdown() {
    if (!initialized_) {
        return;
    }
    ImGui::DestroyContext();
    initialized_ = false;
}

void TradingImGuiApp::beginFrame() {
    if (!initialized_) {
        initialize();
    }
    ImGui::NewFrame();
}

void TradingImGuiApp::render() {
    if (!initialized_) {
        return;
    }

    if (show_demo_window_) {
        bool keep_open = true;
        ImGui::ShowDemoWindow(&keep_open);
        if (!keep_open) {
            show_demo_window_ = false;
        }
    }

    if (ImGui::Begin("MemecoinBot Control Center")) {
        renderStatusSection();
        ImGui::Separator();
        renderOrderEntrySection();
        ImGui::Separator();
        renderRiskSection();
        ImGui::Separator();
        renderLogSection();
    }
    ImGui::End();
}

void TradingImGuiApp::endFrame() {
    if (!initialized_) {
        return;
    }
    ImGui::EndFrame();
    ImGui::Render();
}

void TradingImGuiApp::enqueueLogMessage(std::string message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_messages_.push_back(std::move(message));
    while (log_messages_.size() > max_log_messages_) {
        log_messages_.pop_front();
    }
}

TradingImGuiApp::OrderEntryState& TradingImGuiApp::orderEntryState() {
    return order_entry_;
}

TradingImGuiApp::RiskLimitState& TradingImGuiApp::riskLimitState() {
    return risk_limits_;
}

void TradingImGuiApp::setShowDemoWindow(bool show_demo) {
    show_demo_window_ = show_demo;
}

bool TradingImGuiApp::showDemoWindow() const {
    return show_demo_window_;
}

void TradingImGuiApp::renderStatusSection() {
    bool has_engine = static_cast<bool>(engine_);
    if (!has_engine) {
        ImGui::TextUnformatted("No trading engine attached");
        return;
    }

    bool running = engine_->isRunning();
    ImGui::Text("Engine status: %s", running ? "Running" : "Stopped");

    ImGui::SameLine();
    if (running) {
        if (ImGui::Button("Stop Engine")) {
            engine_->stop();
            enqueueLogMessage("Requested engine stop from UI");
        }
    } else {
        if (ImGui::Button("Start Engine")) {
            engine_->start();
            enqueueLogMessage("Requested engine start from UI");
        }
    }

    bool demo_toggle = show_demo_window_;
    if (ImGui::Checkbox("Show ImGui demo", &demo_toggle)) {
        show_demo_window_ = demo_toggle;
    }
}

void TradingImGuiApp::renderOrderEntrySection() {
    if (!ImGui::CollapsingHeader("Manual Order Entry")) {
        return;
    }

    ImGui::InputText("Symbol", order_entry_.symbol_buffer.data(), order_entry_.symbol_buffer.size());
    ImGui::InputDouble("Quantity", &order_entry_.quantity, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Price", &order_entry_.price, 0.0, 0.0, "%.4f");

    bool can_submit = engine_ != nullptr && engine_->isRunning();
    if (!can_submit) {
        ImGui::TextUnformatted("Order submission available when engine is running");
    }

    if (ImGui::Button("Submit Order") && can_submit) {
        std::string symbol(order_entry_.symbol_buffer.data());
        if (symbol.empty()) {
            enqueueLogMessage("Cannot submit order: symbol is empty");
            return;
        }

        trading::Order order{symbol, order_entry_.quantity, order_entry_.price};
        engine_->submitOrder(order);

        std::ostringstream oss;
        oss << "Submitted order: " << order.symbol << " qty=" << order.quantity
            << " price=" << order.price;
        enqueueLogMessage(oss.str());
    }
}

void TradingImGuiApp::renderRiskSection() {
    if (!ImGui::CollapsingHeader("Risk Configuration")) {
        return;
    }

    ImGui::InputDouble("Max Position", &risk_limits_.max_position, 0.0, 0.0, "%.2f");
    ImGui::InputDouble("Max Exposure", &risk_limits_.max_exposure, 0.0, 0.0, "%.2f");

    if (ImGui::Button("Apply Risk Limits")) {
        if (!engine_) {
            enqueueLogMessage("No engine attached for risk limit update");
            return;
        }

        trading::RiskLimits updated_limits{risk_limits_.max_position, risk_limits_.max_exposure};
        engine_->updateRiskLimits(updated_limits);

        std::ostringstream oss;
        oss << "Updated risk limits: max_position=" << risk_limits_.max_position
            << " max_exposure=" << risk_limits_.max_exposure;
        enqueueLogMessage(oss.str());
    }
}

void TradingImGuiApp::renderLogSection() {
    if (!ImGui::CollapsingHeader("Event Log")) {
        return;
    }

    std::vector<std::string> snapshot;
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        snapshot.assign(log_messages_.begin(), log_messages_.end());
    }

    if (ImGui::BeginChild("LogViewport", ImVec2(0.0f, 180.0f), true)) {
        for (const auto& message : snapshot) {
            ImGui::TextUnformatted(message.c_str());
        }
    }
    ImGui::EndChild();
}

}  // namespace ui

