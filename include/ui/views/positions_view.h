#pragma once

#include <string>
#include <chrono>
#include <unordered_map>

#include "ui/data_subscription.h"
#include "ui/imgui_compat.h"

namespace ui {
struct ThemePalette;
}

namespace ui::views {

struct PositionRow {
    std::string position_id;
    std::string symbol;
    double quantity = 0.0;
    double entry_price = 0.0;
    double mark_price = 0.0;
    double unrealized_pnl = 0.0;
    std::chrono::system_clock::time_point timestamp{};
};

class PositionsView {
  public:
    explicit PositionsView(EngineEventBus &engine_bus);
    ~PositionsView();

    PositionsView(const PositionsView &) = delete;
    PositionsView &operator=(const PositionsView &) = delete;

    void render();
    void set_palette(const ThemePalette *palette) noexcept { m_palette = palette; }
    const std::unordered_map<std::string, PositionRow> &positions() const noexcept { return m_positions; }

  private:
    void handle_position(const PositionUpdate &update);

    EngineEventBus &m_engine_bus;
    SubscriptionToken m_subscription;
    std::unordered_map<std::string, PositionRow> m_positions;
    const ThemePalette *m_palette = nullptr;
};

} // namespace ui::views
