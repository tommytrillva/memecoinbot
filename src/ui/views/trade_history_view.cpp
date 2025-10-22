#include "ui/views/trade_history_view.h"

#include <chrono>

#include "ui/theming.h"

namespace ui::views {

TradeHistoryView::TradeHistoryView(EngineEventBus &engine_bus, std::size_t max_rows)
    : m_engine_bus(engine_bus), m_max_rows(max_rows) {
    m_subscription = m_engine_bus.subscribe_trade([this](const TradeEvent &event) { handle_trade(event); });
}

TradeHistoryView::~TradeHistoryView() { m_subscription.reset(); }

void TradeHistoryView::render() {
    ImGui::TextUnformatted("Trade history");
    ImGui::Separator();

    if (m_trades.empty()) {
        ImGui::TextUnformatted("No trades yet");
        return;
    }

    for (const auto &trade : m_trades) {
        if (m_palette) {
            const ImGui::ImVec4 &color = trade.is_buy ? m_palette->positive : m_palette->negative;
            ImGui::PushStyleColor(ImGui::ImGuiCol_Text, color);
        }
        ImGui::Text("%s %s %.4f @ %.4f", trade.symbol.c_str(), trade.is_buy ? "BUY" : "SELL", trade.quantity,
                   trade.price);
        if (m_palette) {
            ImGui::PopStyleColor();
        }

        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - trade.timestamp).count();
        ImGui::SameLine();
        ImGui::Text("%llds ago", static_cast<long long>(seconds));
        ImGui::Separator();
    }
}

void TradeHistoryView::handle_trade(const TradeEvent &event) {
    TradeRow row;
    row.trade_id = event.trade_id;
    row.symbol = event.symbol;
    row.quantity = event.quantity;
    row.price = event.price;
    row.is_buy = event.is_buy;
    row.timestamp = event.timestamp;

    m_trades.push_front(std::move(row));
    while (m_trades.size() > m_max_rows) {
        m_trades.pop_back();
    }
}

} // namespace ui::views
