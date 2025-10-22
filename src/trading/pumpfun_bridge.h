#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "trading/trading_engine.h"

namespace market_data {
class PumpFunClient;
struct TokenQuote;
}  // namespace market_data

namespace trading {

class PumpFunMarketDataBridge {
public:
    PumpFunMarketDataBridge(market_data::PumpFunClient& client, TradingEngine& engine);
    ~PumpFunMarketDataBridge();

    void start(const std::vector<std::string>& symbols,
               std::chrono::milliseconds interval = std::chrono::milliseconds(1500));
    void stop();

    bool isRunning() const;

private:
    void clearSubscriptions();

    market_data::PumpFunClient& client_;
    TradingEngine& engine_;

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::uint64_t> subscriptions_;
};

}  // namespace trading
