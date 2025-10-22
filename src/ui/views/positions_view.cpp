#include "ui/views/positions_view.h"

#include <algorithm>
#include <vector>

#include "ui/theming.h"

namespace ui::views {

PositionsView::PositionsView(EngineEventBus &engine_bus) : m_engine_bus(engine_bus) {
    m_subscription = m_engine_bus.subscribe_position([this](const PositionUpdate &update) { handle_position(update); });
}

PositionsView::~PositionsView() { m_subscription.reset(); }

void PositionsView::render() {
    ImGui::TextUnformatted("Open positions");
    ImGui::Separator();

    if (m_positions.empty()) {
        ImGui::TextUnformatted("No active positions");
        return;
    }

    std::vector<PositionRow> ordered_positions;
    ordered_positions.reserve(m_positions.size());
    for (const auto &entry : m_positions) {
        ordered_positions.push_back(entry.second);
    }
    std::sort(ordered_positions.begin(), ordered_positions.end(),
              [](const PositionRow &lhs, const PositionRow &rhs) { return lhs.symbol < rhs.symbol; });

    for (const auto &row : ordered_positions) {
        ImGui::Text("%s", row.symbol.c_str());
        ImGui::SameLine();
        ImGui::Text("Qty %.4f", row.quantity);
        ImGui::SameLine();
        ImGui::Text("Entry %.4f", row.entry_price);
        ImGui::SameLine();
        ImGui::Text("Mark %.4f", row.mark_price);

        const double pnl = row.unrealized_pnl;
        if (m_palette) {
            const ImGui::ImVec4 &color = pnl >= 0.0 ? m_palette->positive : m_palette->negative;
            ImGui::PushStyleColor(ImGui::ImGuiCol_Text, color);
        }
        ImGui::Text("PnL %.2f", pnl);
        if (m_palette) {
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
    }
}

void PositionsView::handle_position(const PositionUpdate &update) {
    PositionRow &row = m_positions[update.position_id];
    row.position_id = update.position_id;
    row.symbol = update.symbol;
    row.quantity = update.quantity;
    row.entry_price = update.entry_price;
    row.mark_price = update.mark_price;
    row.unrealized_pnl = update.unrealized_pnl;
    row.timestamp = update.timestamp;
}

} // namespace ui::views
