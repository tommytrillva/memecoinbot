#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include "trading/trading_engine.h"
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
    ~TradingImGuiApp();

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
    struct TradeFeedItem {
        std::string order_id;
        std::string description;
        bool success{false};
        std::chrono::system_clock::time_point timestamp{};
    };

    struct AlertFeedItem {
        std::string title;
        std::string body;
        std::chrono::system_clock::time_point timestamp{};
    };

    struct DashboardSnapshot {
        double wallet_cash_balance{0.0};
        double net_position_quantity{0.0};
        double estimated_portfolio_value{0.0};
        double daily_pnl{0.0};
        double last_price{0.0};
        std::size_t total_orders{0};
        bool has_status{false};
        std::string status_summary;
        std::vector<std::string> status_lines;
        std::chrono::system_clock::time_point status_timestamp{};
        double risk_limit_position{0.0};
        double risk_limit_exposure{0.0};
        bool has_engine{false};
        bool engine_running{false};
        std::vector<TradeFeedItem> trades;
        std::vector<AlertFeedItem> alerts;
    };

    void renderStatusSection(const DashboardSnapshot& snapshot);
    void renderSummaryPanels(const DashboardSnapshot& snapshot);
    void renderMarketOverview();
    void renderPortfolioOverview(const DashboardSnapshot& snapshot);
    void renderOrderEntrySection(const DashboardSnapshot& snapshot);
    void renderRiskSection(const DashboardSnapshot& snapshot);
    void renderActivitySection(const DashboardSnapshot& snapshot);
    void renderLogSection();

    void renderPriceChart();
    void renderOrderBook();
    void renderPositionsPanel(const DashboardSnapshot& snapshot);
    void renderStatusSnapshot(const DashboardSnapshot& snapshot);
    void renderTradeFeed(const DashboardSnapshot& snapshot);
    void renderAlertsFeed(const DashboardSnapshot& snapshot);

    DashboardSnapshot buildDashboardSnapshot() const;

    void refreshStatusFromEngine();
    void updateSyntheticMarketData();

    void handleTradeUpdate(const trading::TradeUpdate& update);
    void handleAlertUpdate(const trading::AlertUpdate& update);
    void handleStatusUpdate(const trading::StatusReport& report);

    static std::string formatRelativeTime(const std::chrono::system_clock::time_point& when);
    double computeNetQuantity(const std::vector<std::string>& positions) const;

    std::shared_ptr<trading::TradingEngine> engine_;
    OrderEntryState order_entry_{};
    RiskLimitState risk_limits_{};

    bool initialized_ = false;
    bool show_demo_window_ = false;

    std::deque<float> price_history_;
    std::size_t max_price_points_ = 360;
    double last_price_ = 24500.0;
    double baseline_price_ = last_price_;

    struct OrderBookLevel {
        double price{0.0};
        double size{0.0};
    };

    std::array<OrderBookLevel, 8> bid_levels_{};
    std::array<OrderBookLevel, 8> ask_levels_{};

    std::mt19937 rng_;
    std::normal_distribution<double> price_noise_{0.0, 12.0};
    std::uniform_real_distribution<double> size_distribution_{0.5, 8.0};

    double wallet_cash_balance_ = 50000.0;
    double net_position_quantity_ = 0.0;
    double estimated_portfolio_value_ = wallet_cash_balance_;
    double daily_pnl_ = 0.0;
    std::size_t total_orders_routed_ = 0;

    bool has_status_snapshot_ = false;
    std::string latest_status_summary_;
    std::vector<std::string> status_lines_;
    std::chrono::system_clock::time_point latest_status_timestamp_{};

    std::deque<TradeFeedItem> trade_feed_;
    std::deque<AlertFeedItem> alert_feed_;
    std::size_t max_feed_items_ = 24;

    std::deque<std::string> log_messages_;
    std::size_t max_log_messages_ = 200;

    std::chrono::steady_clock::time_point last_market_tick_{};
    std::chrono::steady_clock::time_point last_status_fetch_{};
    std::chrono::milliseconds status_poll_interval_{std::chrono::milliseconds(750)};

    std::atomic<bool> manual_status_request_{false};
    bool auto_status_refresh_ = true;
    std::atomic<bool> alive_{true};

    mutable std::mutex data_mutex_;
    mutable std::mutex log_mutex_;
};

}  // namespace ui

