#include "ui/main_window.h"

#include <utility>

#include "ui/imgui_compat.h"

namespace ui {

MainWindow::MainWindow(MarketDataBus &market_data_bus, EngineEventBus &engine_bus)
    : m_theme(create_neon_dark_theme()),
      m_layout(),
      m_price_chart_view(market_data_bus, "MEME/USD"),
      m_positions_view(engine_bus),
      m_trade_history_view(engine_bus) {
    apply_theme();
}

void MainWindow::set_theme(Theme theme) {
    m_theme = std::move(theme);
    m_theme_dirty = true;
}

void MainWindow::apply_theme() {
    m_theme.apply();
    m_positions_view.set_palette(&m_theme.palette);
    m_trade_history_view.set_palette(&m_theme.palette);
    m_theme_dirty = false;
}

void MainWindow::render() {
    if (m_theme_dirty) {
        apply_theme();
    }

    if (!ImGui::Begin("Memecoinbot Trading Desk")) {
        ImGui::End();
        return;
    }

    const ImGui::ImVec2 available = ImGui::GetContentRegionAvail();
    const DashboardLayoutState layout_state = m_layout.compute(available);

    if (layout_state.price_chart_size.y > 0.0f) {
        if (ImGui::BeginChild("PriceChartRegion", layout_state.price_chart_size, true)) {
            m_price_chart_view.render();
        }
        ImGui::EndChild();
    }

    if (layout_state.stack_trades_below_positions) {
        if (layout_state.positions_size.y > 0.0f) {
            ImGui::Separator();
            if (ImGui::BeginChild("PositionsRegion", layout_state.positions_size, true)) {
                m_positions_view.render();
            }
            ImGui::EndChild();
        }

        if (layout_state.trades_size.y > 0.0f) {
            ImGui::Separator();
            if (ImGui::BeginChild("TradesRegion", layout_state.trades_size, true)) {
                m_trade_history_view.render();
            }
            ImGui::EndChild();
        }
    } else {
        ImGui::Separator();
        if (ImGui::BeginChild("PositionsRegion", layout_state.positions_size, true)) {
            m_positions_view.render();
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, m_layout.gutter());
        ImGui::ImVec2 trade_size = layout_state.trades_size;
        if (trade_size.x <= 0.0f) {
            trade_size.x = ImGui::GetContentRegionAvail().x;
        }
        if (ImGui::BeginChild("TradesRegion", trade_size, true)) {
            m_trade_history_view.render();
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace ui
