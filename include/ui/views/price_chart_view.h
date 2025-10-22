#pragma once

#include <deque>
#include <chrono>
#include <string>
#include <vector>

#include "ui/data_subscription.h"
#include "ui/imgui_compat.h"

namespace ui::views {

class PriceChartView {
  public:
    PriceChartView(MarketDataBus &market_data, std::string symbol, std::size_t max_samples = 512);
    ~PriceChartView();

    PriceChartView(const PriceChartView &) = delete;
    PriceChartView &operator=(const PriceChartView &) = delete;

    void render();
    void set_symbol(std::string symbol);

  private:
    void subscribe();
    void handle_price(const PricePoint &point);
    void refresh_plot_cache();

    MarketDataBus &m_market_data;
    std::string m_symbol;
    std::size_t m_max_samples;
    SubscriptionToken m_subscription;
    std::deque<float> m_price_history;
    std::deque<float> m_volume_history;
    std::deque<std::chrono::system_clock::time_point> m_timestamps;
    std::vector<float> m_plot_cache;
    bool m_plot_dirty = true;
    float m_min_price = 0.0f;
    float m_max_price = 0.0f;
};

} // namespace ui::views
