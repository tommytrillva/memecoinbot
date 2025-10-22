#include "ui/imgui_trading_app.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

namespace ui {
namespace {
constexpr char kDefaultSymbol[] = "BTC-USD";
constexpr double kDefaultMaxPosition = 25.0;
constexpr double kDefaultMaxExposure = 125.0;
constexpr std::chrono::milliseconds kSyntheticTickInterval{250};
constexpr std::size_t kMaxDisplayedFeedItems = 10;
}  // namespace

TradingImGuiApp::TradingImGuiApp() : rng_(std::random_device{}()) {
    std::fill(order_entry_.symbol_buffer.begin(), order_entry_.symbol_buffer.end(), '\0');
    const auto default_length =
        std::min(order_entry_.symbol_buffer.size() - 1, sizeof(kDefaultSymbol));
    std::copy_n(kDefaultSymbol, default_length, order_entry_.symbol_buffer.begin());

    risk_limits_.max_position = kDefaultMaxPosition;
    risk_limits_.max_exposure = kDefaultMaxExposure;

    baseline_price_ = last_price_;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        price_history_.push_back(static_cast<float>(last_price_));
    }
}

TradingImGuiApp::~TradingImGuiApp() { alive_.store(false); }

void TradingImGuiApp::attachEngine(std::shared_ptr<trading::TradingEngine> engine) {
    engine_ = std::move(engine);
    if (!engine_) {
        return;
    }

    engine_->subscribeToTradeUpdates(
        [this](const trading::TradeUpdate& update) { handleTradeUpdate(update); });
    engine_->subscribeToAlerts(
        [this](const trading::AlertUpdate& alert) { handleAlertUpdate(alert); });
    engine_->subscribeToStatusUpdates(
        [this](const trading::StatusReport& report) { handleStatusUpdate(report); });

    refreshStatusFromEngine();
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

    updateSyntheticMarketData();
    refreshStatusFromEngine();

    auto snapshot = buildDashboardSnapshot();
    snapshot.has_engine = static_cast<bool>(engine_);
    snapshot.engine_running = snapshot.has_engine && engine_->isRunning();

    if (show_demo_window_) {
        bool keep_open = true;
        ImGui::ShowDemoWindow(&keep_open);
        if (!keep_open) {
            show_demo_window_ = false;
        }
    }

    if (ImGui::Begin("MemecoinBot Control Center")) {
        renderStatusSection(snapshot);
        ImGui::Spacing();
        ImGui::Separator();

        renderSummaryPanels(snapshot);
        ImGui::Spacing();
        ImGui::Separator();

        renderMarketOverview();
        ImGui::Spacing();
        ImGui::Separator();

        renderPortfolioOverview(snapshot);
        ImGui::Spacing();
        ImGui::Separator();

        renderOrderEntrySection(snapshot);
        ImGui::Spacing();
        ImGui::Separator();

        renderRiskSection(snapshot);
        ImGui::Spacing();
        ImGui::Separator();

        renderActivitySection(snapshot);
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

TradingImGuiApp::OrderEntryState& TradingImGuiApp::orderEntryState() { return order_entry_; }

TradingImGuiApp::RiskLimitState& TradingImGuiApp::riskLimitState() { return risk_limits_; }

void TradingImGuiApp::setShowDemoWindow(bool show_demo) { show_demo_window_ = show_demo; }

bool TradingImGuiApp::showDemoWindow() const { return show_demo_window_; }

void TradingImGuiApp::renderStatusSection(const DashboardSnapshot& snapshot) {
    if (!snapshot.has_engine) {
        ImGui::TextUnformatted("No trading engine attached");
        return;
    }

    ImGui::Text("Engine status: %s", snapshot.engine_running ? "Running" : "Stopped");
    ImGui::SameLine();
    if (snapshot.engine_running) {
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

    ImGui::SameLine();
    if (ImGui::Checkbox("Auto status", &auto_status_refresh_)) {
        manual_status_request_.store(true);
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Snapshot")) {
        manual_status_request_.store(true);
        refreshStatusFromEngine();
    }

    bool demo_toggle = show_demo_window_;
    if (ImGui::Checkbox("Show ImGui demo", &demo_toggle)) {
        show_demo_window_ = demo_toggle;
    }

    if (snapshot.has_status) {
        ImGui::Text("%s", snapshot.status_summary.c_str());
        const auto freshness = formatRelativeTime(snapshot.status_timestamp);
        ImGui::Text("Last update: %s", freshness.c_str());
    }
}

void TradingImGuiApp::renderSummaryPanels(const DashboardSnapshot& snapshot) {
    if (!ImGui::BeginChild("SummaryRow", ImVec2(0.0f, 140.0f), false)) {
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float third = available.x / 3.0f - 12.0f;

    if (ImGui::BeginChild("WalletCard", ImVec2(third, 0.0f), true)) {
        ImGui::TextUnformatted("Wallet Balances");
        ImGui::Separator();
        ImGui::Text("Cash: %.2f", snapshot.wallet_cash_balance);
        ImGui::Text("Net Tokens: %.4f", snapshot.net_position_quantity);
        ImGui::Text("Portfolio: %.2f", snapshot.estimated_portfolio_value);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("RiskCard", ImVec2(third, 0.0f), true)) {
        ImGui::TextUnformatted("Risk Utilization");
        ImGui::Separator();
        ImGui::Text("Position Limit: %.2f", snapshot.risk_limit_position);
        ImGui::Text("Exposure Limit: %.2f", snapshot.risk_limit_exposure);
        double ratio = 0.0;
        if (snapshot.risk_limit_exposure > 0.0) {
            ratio = std::clamp(snapshot.estimated_portfolio_value / snapshot.risk_limit_exposure, 0.0, 1.0);
        }
        ImGui::ProgressBar(static_cast<float>(ratio));
        ImGui::Text("Utilization: %.1f%%", ratio * 100.0);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("PerformanceCard", ImVec2(0.0f, 0.0f), true)) {
        ImGui::TextUnformatted("Performance");
        ImGui::Separator();
        ImGui::Text("Synthetic Price: %.2f", snapshot.last_price);
        ImGui::Text("Daily P&L: %.2f", snapshot.daily_pnl);
        ImGui::Text("Orders Routed: %zu", snapshot.total_orders);
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

void TradingImGuiApp::renderMarketOverview() {
    if (!ImGui::CollapsingHeader("Market Overview")) {
        return;
    }

    if (!ImGui::BeginChild("MarketRow", ImVec2(0.0f, 240.0f), false)) {
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float chart_width = available.x * 0.62f;

    if (ImGui::BeginChild("PriceChartPanel", ImVec2(chart_width, 0.0f), true)) {
        ImGui::TextUnformatted("Live Price (synthetic)");
        ImGui::Separator();
        renderPriceChart();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("OrderBookPanel", ImVec2(0.0f, 0.0f), true)) {
        ImGui::TextUnformatted("Synthetic Order Book");
        ImGui::Separator();
        renderOrderBook();
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

void TradingImGuiApp::renderPortfolioOverview(const DashboardSnapshot& snapshot) {
    if (!ImGui::CollapsingHeader("Portfolio Overview")) {
        return;
    }

    if (!ImGui::BeginChild("PortfolioRow", ImVec2(0.0f, 200.0f), false)) {
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float positions_width = available.x * 0.55f;

    if (ImGui::BeginChild("PositionsPanel", ImVec2(positions_width, 0.0f), true)) {
        renderPositionsPanel(snapshot);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("StatusPanel", ImVec2(0.0f, 0.0f), true)) {
        renderStatusSnapshot(snapshot);
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

void TradingImGuiApp::renderOrderEntrySection(const DashboardSnapshot& snapshot) {
    if (!ImGui::CollapsingHeader("Manual Order Entry")) {
        return;
    }

    ImGui::Text("Last Price: %.2f", snapshot.last_price);
    ImGui::Text("Net Position: %.4f", snapshot.net_position_quantity);

    ImGui::InputText("Symbol", order_entry_.symbol_buffer.data(), order_entry_.symbol_buffer.size());
    ImGui::InputDouble("Quantity", &order_entry_.quantity, 0.0, 0.0, "%.4f");
    ImGui::InputDouble("Price", &order_entry_.price, 0.0, 0.0, "%.4f");

    ImGui::Spacing();
    ImGui::TextUnformatted("Quick Actions");
    const double default_size = std::max(0.01, snapshot.risk_limit_position * 0.25);
    if (ImGui::Button("Buy 25% Limit")) {
        order_entry_.quantity = default_size;
        order_entry_.price = snapshot.last_price * 0.995;
    }
    ImGui::SameLine();
    if (ImGui::Button("Sell 25% Limit")) {
        order_entry_.quantity = -default_size;
        order_entry_.price = snapshot.last_price * 1.005;
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten Position")) {
        order_entry_.quantity = -snapshot.net_position_quantity;
        order_entry_.price = 0.0;
    }

    bool can_submit = snapshot.engine_running;
    if (!can_submit) {
        ImGui::TextUnformatted("Order submission available when engine is running");
    }

    if (ImGui::Button("Submit Order") && can_submit) {
        std::string symbol(order_entry_.symbol_buffer.data());
        if (symbol.empty()) {
            enqueueLogMessage("Cannot submit order: symbol is empty");
            return;
        }

        const double raw_quantity = order_entry_.quantity;
        if (raw_quantity == 0.0) {
            enqueueLogMessage("Cannot submit order: quantity must be non-zero");
            return;
        }

        const bool is_buy = raw_quantity > 0.0;
        trading::OrderRequest request;
        request.symbol = std::move(symbol);
        request.quantity = std::abs(raw_quantity);
        if (order_entry_.price > 0.0) {
            request.limitPrice = order_entry_.price;
        }

        const auto receipt = is_buy ? engine_->buy(request) : engine_->sell(request);

        std::ostringstream oss;
        oss << "Submitted " << (is_buy ? "buy" : "sell") << " order for " << request.symbol
            << " qty=" << request.quantity;
        if (request.limitPrice) {
            oss << " @ " << *request.limitPrice;
        }
        oss << "\nEngine response: " << receipt.message;
        if (!receipt.orderId.empty()) {
            oss << " (" << receipt.orderId << ")";
        }
        enqueueLogMessage(oss.str());
    }
}

void TradingImGuiApp::renderRiskSection(const DashboardSnapshot& snapshot) {
    if (!ImGui::CollapsingHeader("Risk Configuration")) {
        return;
    }

    ImGui::InputDouble("Max Position", &risk_limits_.max_position, 0.0, 0.0, "%.2f");
    ImGui::InputDouble("Max Exposure", &risk_limits_.max_exposure, 0.0, 0.0, "%.2f");

    double utilization = 0.0;
    if (risk_limits_.max_exposure > 0.0) {
        utilization = std::clamp(snapshot.estimated_portfolio_value / risk_limits_.max_exposure, 0.0, 1.0);
    }
    ImGui::ProgressBar(static_cast<float>(utilization));
    ImGui::Text("Exposure utilization: %.1f%%", utilization * 100.0);

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

    ImGui::SameLine();
    if (ImGui::Button("Reset Defaults")) {
        risk_limits_.max_position = kDefaultMaxPosition;
        risk_limits_.max_exposure = kDefaultMaxExposure;
    }
}

void TradingImGuiApp::renderActivitySection(const DashboardSnapshot& snapshot) {
    if (!ImGui::CollapsingHeader("Control Plane Activity")) {
        return;
    }

    if (ImGui::BeginChild("ActivityFeeds", ImVec2(0.0f, 220.0f), false)) {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float half_width = available.x * 0.5f - 8.0f;

        if (ImGui::BeginChild("TradeFeed", ImVec2(half_width, 0.0f), true)) {
            renderTradeFeed(snapshot);
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (ImGui::BeginChild("AlertsFeed", ImVec2(0.0f, 0.0f), true)) {
            renderAlertsFeed(snapshot);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextUnformatted("Event Log");
    renderLogSection();
}

void TradingImGuiApp::renderLogSection() {
    std::vector<std::string> snapshot;
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        snapshot.assign(log_messages_.begin(), log_messages_.end());
    }

    if (ImGui::BeginChild("LogViewport", ImVec2(0.0f, 160.0f), true)) {
        for (const auto& message : snapshot) {
            ImGui::TextUnformatted(message.c_str());
        }
    }
    ImGui::EndChild();
}

void TradingImGuiApp::renderPriceChart() {
    std::vector<float> samples;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        samples.assign(price_history_.begin(), price_history_.end());
    }

    if (samples.empty()) {
        ImGui::TextUnformatted("No price data");
        return;
    }

    const auto [min_it, max_it] = std::minmax_element(samples.begin(), samples.end());
    float min_price = *min_it;
    float max_price = *max_it;
    if (std::fabs(max_price - min_price) < 1e-3f) {
        max_price = min_price + 1.0f;
    }
    ImGui::PlotLines("##PriceSeries", samples.data(), static_cast<int>(samples.size()), 0, nullptr,
                     min_price, max_price, ImVec2(0.0f, 140.0f));
}

void TradingImGuiApp::renderOrderBook() {
    std::array<OrderBookLevel, 8> bids;
    std::array<OrderBookLevel, 8> asks;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        bids = bid_levels_;
        asks = ask_levels_;
    }

    ImGui::TextUnformatted("Asks");
    ImGui::Separator();
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        ImGui::Text("%.2f | %.3f", it->price, it->size);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Bids");
    ImGui::Separator();
    for (const auto& level : bids) {
        ImGui::Text("%.2f | %.3f", level.price, level.size);
    }
}

void TradingImGuiApp::renderPositionsPanel(const DashboardSnapshot& snapshot) {
    ImGui::TextUnformatted("Open Positions");
    ImGui::Separator();
    if (snapshot.status_lines.empty()) {
        ImGui::TextUnformatted("No open positions");
        return;
    }

    int displayed = 0;
    for (const auto& line : snapshot.status_lines) {
        if (displayed++ >= 12) {
            ImGui::TextUnformatted("…");
            break;
        }
        ImGui::TextUnformatted(line.c_str());
    }
}

void TradingImGuiApp::renderStatusSnapshot(const DashboardSnapshot& snapshot) {
    ImGui::TextUnformatted("Portfolio Status");
    ImGui::Separator();
    if (!snapshot.has_status) {
        ImGui::TextUnformatted("Awaiting engine snapshot…");
        return;
    }

    ImGui::TextUnformatted(snapshot.status_summary.c_str());
    const auto freshness = formatRelativeTime(snapshot.status_timestamp);
    ImGui::Text("Updated %s", freshness.c_str());
}

void TradingImGuiApp::renderTradeFeed(const DashboardSnapshot& snapshot) {
    ImGui::TextUnformatted("Trade Updates");
    ImGui::Separator();
    if (snapshot.trades.empty()) {
        ImGui::TextUnformatted("No trade activity yet");
        return;
    }

    const ImVec4 success_colour{0.25f, 0.85f, 0.45f, 1.0f};
    const ImVec4 failure_colour{0.95f, 0.45f, 0.35f, 1.0f};

    std::size_t rendered = 0;
    for (const auto& trade : snapshot.trades) {
        if (rendered++ >= kMaxDisplayedFeedItems) {
            ImGui::TextUnformatted("…");
            break;
        }

        const auto& colour = trade.success ? success_colour : failure_colour;
        ImGui::TextColored(colour, "%s", trade.description.c_str());
        std::ostringstream meta;
        if (!trade.order_id.empty()) {
            meta << trade.order_id << " | ";
        }
        meta << formatRelativeTime(trade.timestamp);
        ImGui::Text("%s", meta.str().c_str());
        ImGui::Separator();
    }
}

void TradingImGuiApp::renderAlertsFeed(const DashboardSnapshot& snapshot) {
    ImGui::TextUnformatted("Risk & Alert Stream");
    ImGui::Separator();
    if (snapshot.alerts.empty()) {
        ImGui::TextUnformatted("No alerts raised");
        return;
    }

    const ImVec4 alert_colour{0.95f, 0.75f, 0.25f, 1.0f};
    std::size_t rendered = 0;
    for (const auto& alert : snapshot.alerts) {
        if (rendered++ >= kMaxDisplayedFeedItems) {
            ImGui::TextUnformatted("…");
            break;
        }
        ImGui::TextColored(alert_colour, "%s", alert.title.c_str());
        ImGui::TextWrapped("%s", alert.body.c_str());
        ImGui::Text("%s", formatRelativeTime(alert.timestamp).c_str());
        ImGui::Separator();
    }
}

TradingImGuiApp::DashboardSnapshot TradingImGuiApp::buildDashboardSnapshot() const {
    DashboardSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        snapshot.wallet_cash_balance = wallet_cash_balance_;
        snapshot.net_position_quantity = net_position_quantity_;
        snapshot.estimated_portfolio_value = estimated_portfolio_value_;
        snapshot.daily_pnl = daily_pnl_;
        snapshot.last_price = last_price_;
        snapshot.total_orders = total_orders_routed_;
        snapshot.has_status = has_status_snapshot_;
        snapshot.status_summary = latest_status_summary_;
        snapshot.status_lines = status_lines_;
        snapshot.status_timestamp = latest_status_timestamp_;
        snapshot.trades.assign(trade_feed_.begin(), trade_feed_.end());
        snapshot.alerts.assign(alert_feed_.begin(), alert_feed_.end());
    }
    snapshot.risk_limit_position = risk_limits_.max_position;
    snapshot.risk_limit_exposure = risk_limits_.max_exposure;
    return snapshot;
}

void TradingImGuiApp::refreshStatusFromEngine() {
    auto engine = engine_;
    if (!engine) {
        return;
    }

    const bool manual_request = manual_status_request_.exchange(false);
    if (!auto_status_refresh_ && !manual_request) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!manual_request && last_status_fetch_.time_since_epoch().count() != 0 &&
        now - last_status_fetch_ < status_poll_interval_) {
        return;
    }

    last_status_fetch_ = now;
    handleStatusUpdate(engine->status(std::nullopt));
}

void TradingImGuiApp::updateSyntheticMarketData() {
    const auto now = std::chrono::steady_clock::now();
    if (last_market_tick_.time_since_epoch().count() == 0) {
        last_market_tick_ = now;
    }
    if (now - last_market_tick_ < kSyntheticTickInterval) {
        return;
    }
    last_market_tick_ = now;

    std::lock_guard<std::mutex> lock(data_mutex_);

    if (price_history_.empty()) {
        price_history_.push_back(static_cast<float>(last_price_));
    }

    const double noise = price_noise_(rng_);
    last_price_ = std::max(1.0, last_price_ + noise);
    price_history_.push_back(static_cast<float>(last_price_));
    if (price_history_.size() > max_price_points_) {
        price_history_.pop_front();
    }

    const double mid = last_price_;
    for (std::size_t i = 0; i < bid_levels_.size(); ++i) {
        const double step = 0.5 * (static_cast<double>(i) + 1.0);
        bid_levels_[i].price = mid - step;
        bid_levels_[i].size = std::max(0.1, size_distribution_(rng_));
        ask_levels_[i].price = mid + step;
        ask_levels_[i].size = std::max(0.1, size_distribution_(rng_));
    }

    estimated_portfolio_value_ = wallet_cash_balance_ + net_position_quantity_ * last_price_;
    daily_pnl_ = net_position_quantity_ * (last_price_ - baseline_price_);
}

void TradingImGuiApp::handleTradeUpdate(const trading::TradeUpdate& update) {
    if (!alive_.load()) {
        return;
    }

    TradeFeedItem item;
    item.order_id = update.orderId;
    item.description = update.message;
    item.success = update.success;
    item.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        trade_feed_.push_front(item);
        if (trade_feed_.size() > max_feed_items_) {
            trade_feed_.pop_back();
        }
        if (update.success) {
            ++total_orders_routed_;
        }
    }

    std::ostringstream oss;
    oss << "Trade update [" << (item.order_id.empty() ? "n/a" : item.order_id) << "]: "
        << item.description;
    enqueueLogMessage(oss.str());
}

void TradingImGuiApp::handleAlertUpdate(const trading::AlertUpdate& alert) {
    if (!alive_.load()) {
        return;
    }

    AlertFeedItem item;
    item.title = alert.title;
    item.body = alert.body;
    item.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        alert_feed_.push_front(item);
        if (alert_feed_.size() > max_feed_items_) {
            alert_feed_.pop_back();
        }
    }

    std::ostringstream oss;
    oss << "Alert: " << item.title << " - " << item.body;
    enqueueLogMessage(oss.str());
}

void TradingImGuiApp::handleStatusUpdate(const trading::StatusReport& report) {
    if (!alive_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    has_status_snapshot_ = true;
    latest_status_summary_ = report.summary;
    status_lines_ = report.positions;
    latest_status_timestamp_ = std::chrono::system_clock::now();
    net_position_quantity_ = computeNetQuantity(status_lines_);
    estimated_portfolio_value_ = wallet_cash_balance_ + net_position_quantity_ * last_price_;
    daily_pnl_ = net_position_quantity_ * (last_price_ - baseline_price_);
}

std::string TradingImGuiApp::formatRelativeTime(
    const std::chrono::system_clock::time_point& when) {
    if (when.time_since_epoch().count() == 0) {
        return "n/a";
    }

    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto diff = now - when;
    const auto seconds = duration_cast<std::chrono::seconds>(diff).count();

    if (seconds <= 0) {
        return "just now";
    }
    if (seconds < 60) {
        return std::to_string(seconds) + "s ago";
    }
    const auto minutes = seconds / 60;
    if (minutes < 60) {
        return std::to_string(minutes) + "m ago";
    }
    const auto hours = minutes / 60;
    if (hours < 24) {
        return std::to_string(hours) + "h ago";
    }
    const auto days = hours / 24;
    return std::to_string(days) + "d ago";
}

double TradingImGuiApp::computeNetQuantity(const std::vector<std::string>& positions) const {
    double net = 0.0;
    for (const auto& entry : positions) {
        const auto colon = entry.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string value = entry.substr(colon + 1);
        char* end = nullptr;
        const double parsed = std::strtod(value.c_str(), &end);
        if (end == value.c_str()) {
            continue;
        }
        net += parsed;
    }
    return net;
}

}  // namespace ui
