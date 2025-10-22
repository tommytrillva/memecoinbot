#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "trading/trading_engine.h"

namespace trading {

class RiskManagedEngine : public TradingEngine {
public:
    RiskManagedEngine();
    explicit RiskManagedEngine(RiskLimits limits);
    ~RiskManagedEngine() override;

    RiskManagedEngine(const RiskManagedEngine&) = delete;
    RiskManagedEngine& operator=(const RiskManagedEngine&) = delete;

    RiskManagedEngine(RiskManagedEngine&&) = delete;
    RiskManagedEngine& operator=(RiskManagedEngine&&) = delete;

    void start() override;
    void stop() override;
    bool isRunning() const override;

    void updateRiskLimits(const RiskLimits& limits) override;

    OrderReceipt buy(const OrderRequest& request) override;
    OrderReceipt sell(const OrderRequest& request) override;
    StatusReport status(const std::optional<std::string>& symbol) const override;

    void updateMarkPrice(const std::string& symbol, double price) override;

    void subscribeToTradeUpdates(TradeCallback callback) override;
    void subscribeToAlerts(AlertCallback callback) override;
    void subscribeToStatusUpdates(StatusCallback callback) override;

private:
    struct Order {
        std::string orderId;
        std::string symbol;
        double quantity{0.0};
        std::optional<double> limitPrice;
        enum class Side { Buy, Sell } side{Side::Buy};
    };

    OrderReceipt submitOrder(const OrderRequest& request, Order::Side side);

    void executionLoop();
    void routePendingOrders(std::vector<Order>& orders);
    void handleOrderRouting(const Order& order);
    void updatePositionTracking(const Order& order);
    bool applyRiskChecks(const Order& order) const;
    void evaluateAggregateRisk() const;

    void notifyTradeUpdate(const TradeUpdate& update) const;
    void notifyAlert(const AlertUpdate& alert) const;
    void notifyStatusUpdate(const StatusReport& report) const;

    std::string generateOrderId();

    StatusReport buildStatusReport(const std::optional<std::string>& symbol) const;

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::thread worker_;

    std::vector<Order> orderQueue_;
    std::unordered_map<std::string, double> positions_;
    std::unordered_map<std::string, double> mark_prices_;
    RiskLimits riskLimits_;

    mutable std::mutex callbacksMutex_;
    std::vector<TradeCallback> tradeSubscribers_;
    std::vector<AlertCallback> alertSubscribers_;
    std::vector<StatusCallback> statusSubscribers_;

    std::atomic<std::uint64_t> orderCounter_{0};
};

}  // namespace trading
