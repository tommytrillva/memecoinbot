#pragma once

#include <deque>
#include <chrono>
#include <string>
#include <vector>

#include "ui/data_subscription.h"
#include "ui/imgui_compat.h"

namespace ui {
struct ThemePalette;
}

namespace ui::views {

struct TradeRow {
    std::string trade_id;
    std::string symbol;
    double quantity = 0.0;
    double price = 0.0;
    bool is_buy = true;
    std::chrono::system_clock::time_point timestamp{};
};

class TradeHistoryView {
  public:
    explicit TradeHistoryView(EngineEventBus &engine_bus, std::size_t max_rows = 200);
    ~TradeHistoryView();

    TradeHistoryView(const TradeHistoryView &) = delete;
    TradeHistoryView &operator=(const TradeHistoryView &) = delete;

    void render();
    void set_palette(const ThemePalette *palette) noexcept { m_palette = palette; }

  private:
    void handle_trade(const TradeEvent &event);

    EngineEventBus &m_engine_bus;
    std::size_t m_max_rows;
    SubscriptionToken m_subscription;
    std::deque<TradeRow> m_trades;
    const ThemePalette *m_palette = nullptr;
};

} // namespace ui::views
