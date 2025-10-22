#pragma once

#include <string>

#include "ui/data_subscription.h"
#include "ui/layout.h"
#include "ui/theming.h"
#include "ui/views/positions_view.h"
#include "ui/views/price_chart_view.h"
#include "ui/views/trade_history_view.h"

namespace ui {

class MainWindow {
  public:
    MainWindow(MarketDataBus &market_data_bus, EngineEventBus &engine_bus);

    void render();

    void set_theme(Theme theme);
    const Theme &theme() const noexcept { return m_theme; }
    DashboardLayout &layout() noexcept { return m_layout; }
    const DashboardLayout &layout() const noexcept { return m_layout; }

  private:
    void apply_theme();

    Theme m_theme;
    DashboardLayout m_layout;
    bool m_theme_dirty = true;

    views::PriceChartView m_price_chart_view;
    views::PositionsView m_positions_view;
    views::TradeHistoryView m_trade_history_view;
};

} // namespace ui
