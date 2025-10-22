#include "ui/main_window.h"

#include <chrono>

int main() {
    ui::MarketDataBus market_data_bus;
    ui::EngineEventBus engine_bus;

    ui::MainWindow main_window(market_data_bus, engine_bus);

    const auto now = std::chrono::system_clock::now();
    market_data_bus.publish_price({"MEME/USD", 0.000042, 1200.0, now});

    ui::PositionUpdate position_update{
        .position_id = "pos-1",
        .symbol = "MEME/USD",
        .quantity = 1200.0,
        .entry_price = 0.000030,
        .mark_price = 0.000042,
        .unrealized_pnl = (0.000042 - 0.000030) * 1200.0,
        .timestamp = now,
    };
    engine_bus.publish_position(position_update);

    ui::TradeEvent trade_event{
        .trade_id = "trade-1",
        .symbol = "MEME/USD",
        .quantity = 600.0,
        .price = 0.000041,
        .is_buy = true,
        .timestamp = now,
    };
    engine_bus.publish_trade(trade_event);

    main_window.render();

    return 0;
}
