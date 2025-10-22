#include "trading/pumpfun_bridge.h"

#include <utility>

#include "common/logging.h"
#include "market_data/pumpfun_client.h"

namespace trading {

PumpFunMarketDataBridge::PumpFunMarketDataBridge(market_data::PumpFunClient& client,
                                                 TradingEngine& engine)
    : client_(client), engine_(engine) {}

PumpFunMarketDataBridge::~PumpFunMarketDataBridge() {
    stop();
}

void PumpFunMarketDataBridge::start(const std::vector<std::string>& symbols,
                                    std::chrono::milliseconds interval) {
    if (symbols.empty()) {
        return;
    }

    if (running_.exchange(true)) {
        stop();
        running_.store(true);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.clear();

    for (const auto& symbol : symbols) {
        try {
            const auto id = client_.subscribeToQuotes(
                symbol,
                [this](const market_data::TokenQuote& quote) {
                    if (quote.price <= 0.0) {
                        LOG_WARN("Received non-positive Pump.fun price for " + quote.mint);
                        return;
                    }
                    engine_.updateMarkPrice(quote.mint, quote.price);
                },
                interval);
            subscriptions_.emplace(symbol, id);
        } catch (const std::exception& ex) {
            LOG_ERROR(std::string("Failed to subscribe to Pump.fun quotes for ") + symbol +
                      ": " + ex.what());
        }
    }
}

void PumpFunMarketDataBridge::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    clearSubscriptions();
}

bool PumpFunMarketDataBridge::isRunning() const {
    return running_.load();
}

void PumpFunMarketDataBridge::clearSubscriptions() {
    std::unordered_map<std::string, std::uint64_t> local;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        local.swap(subscriptions_);
    }

    for (const auto& [symbol, id] : local) {
        (void)symbol;
        client_.unsubscribe(id);
    }
}

}  // namespace trading
