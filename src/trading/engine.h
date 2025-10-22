#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace trading {

struct Order {
    std::string symbol;
    double quantity{};
    double price{};
};

struct RiskLimits {
    double maxPosition = 0.0;
    double maxExposure = 0.0;
};

class TradingEngine {
public:
    TradingEngine();
    explicit TradingEngine(RiskLimits limits);
    ~TradingEngine();

    TradingEngine(const TradingEngine&) = delete;
    TradingEngine& operator=(const TradingEngine&) = delete;

    TradingEngine(TradingEngine&&) = delete;
    TradingEngine& operator=(TradingEngine&&) = delete;

    void start();
    void stop();
    bool isRunning() const;

    void submitOrder(const Order& order);
    void updateRiskLimits(const RiskLimits& limits);

private:
    void executionLoop();
    void routePendingOrders(std::vector<Order>& orders);
    void handleOrderRouting(const Order& order);
    void updatePositionTracking(const Order& order);
    bool applyRiskChecks(const Order& order) const;
    void evaluateAggregateRisk() const;

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::thread worker_;

    std::vector<Order> orderQueue_;
    std::unordered_map<std::string, double> positions_;
    RiskLimits riskLimits_;
};

}  // namespace trading
