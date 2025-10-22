#include "ui/views/price_chart_view.h"

#include <algorithm>
#include <chrono>

namespace ui::views {

PriceChartView::PriceChartView(MarketDataBus &market_data, std::string symbol, std::size_t max_samples)
    : m_market_data(market_data), m_symbol(std::move(symbol)), m_max_samples(max_samples) {
    subscribe();
}

PriceChartView::~PriceChartView() { m_subscription.reset(); }

void PriceChartView::set_symbol(std::string symbol) {
    if (symbol == m_symbol) {
        return;
    }

    m_subscription.reset();
    m_symbol = std::move(symbol);
    m_price_history.clear();
    m_volume_history.clear();
    m_timestamps.clear();
    m_plot_cache.clear();
    m_plot_dirty = true;
    subscribe();
}

void PriceChartView::render() {
    ImGui::Text("%s price", m_symbol.c_str());
    ImGui::Separator();

    if (m_price_history.empty()) {
        ImGui::TextUnformatted("Waiting for market data...");
        return;
    }

    refresh_plot_cache();

    float min_price = m_min_price;
    float max_price = m_max_price;
    if (min_price == max_price) {
        const float delta = std::max(1.0f, max_price * 0.05f);
        min_price -= delta;
        max_price += delta;
    }

    ImGui::PlotLines("##price_history", m_plot_cache.data(), static_cast<int>(m_plot_cache.size()), 0, nullptr, min_price,
                      max_price, ImGui::ImVec2(0.0f, 240.0f));

    const float latest_price = m_price_history.back();
    ImGui::Text("Last: %.4f", latest_price);

    if (!m_timestamps.empty()) {
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_timestamps.back()).count();
        ImGui::SameLine();
        ImGui::Text("Updated %llds ago", static_cast<long long>(seconds));
    }
}

void PriceChartView::subscribe() {
    if (m_symbol.empty()) {
        return;
    }

    m_subscription = m_market_data.subscribe_price(m_symbol, [this](const PricePoint &point) { handle_price(point); });
}

void PriceChartView::handle_price(const PricePoint &point) {
    const float price = static_cast<float>(point.price);
    const float volume = static_cast<float>(point.volume);

    m_price_history.push_back(price);
    m_volume_history.push_back(volume);
    m_timestamps.push_back(point.timestamp);

    while (m_price_history.size() > m_max_samples) {
        m_price_history.pop_front();
        m_volume_history.pop_front();
        m_timestamps.pop_front();
    }

    m_plot_dirty = true;

    if (m_price_history.size() == 1) {
        m_min_price = price;
        m_max_price = price;
    } else {
        m_min_price = std::min(m_min_price, price);
        m_max_price = std::max(m_max_price, price);
    }
}

void PriceChartView::refresh_plot_cache() {
    if (!m_plot_dirty) {
        return;
    }

    m_plot_cache.assign(m_price_history.begin(), m_price_history.end());
    if (!m_plot_cache.empty()) {
        auto [min_it, max_it] = std::minmax_element(m_plot_cache.begin(), m_plot_cache.end());
        m_min_price = *min_it;
        m_max_price = *max_it;
    }

    m_plot_dirty = false;
}

} // namespace ui::views
