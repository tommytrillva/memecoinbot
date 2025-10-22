#include "trading/engine.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <utility>

namespace trading {
namespace {
constexpr auto kEngineSleepInterval = std::chrono::milliseconds(100);
}

TradingEngine::TradingEngine() : TradingEngine(RiskLimits{}) {}

TradingEngine::TradingEngine(RiskLimits limits) : riskLimits_(std::move(limits)) {}

TradingEngine::~TradingEngine() {
    stop();
}

void TradingEngine::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;  // already running
    }

    worker_ = std::thread(&TradingEngine::executionLoop, this);
}

void TradingEngine::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

bool TradingEngine::isRunning() const {
    return running_.load();
}

void TradingEngine::submitOrder(const Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    orderQueue_.push_back(order);
}

void TradingEngine::updateRiskLimits(const RiskLimits& limits) {
    std::lock_guard<std::mutex> lock(mutex_);
    riskLimits_ = limits;
}

void TradingEngine::executionLoop() {
    while (running_.load()) {
        std::vector<Order> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending.swap(orderQueue_);
        }

        if (!pending.empty()) {
            routePendingOrders(pending);
        }

        evaluateAggregateRisk();
        std::this_thread::sleep_for(kEngineSleepInterval);
    }

    // Drain any remaining orders when shutting down.
    std::vector<Order> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(orderQueue_);
    }
    if (!pending.empty()) {
        routePendingOrders(pending);
    }
}

void TradingEngine::routePendingOrders(std::vector<Order>& orders) {
    for (const auto& order : orders) {
        if (!applyRiskChecks(order)) {
            std::cout << "Risk control rejected order for symbol " << order.symbol << "\n";
            continue;
        }

        handleOrderRouting(order);
        updatePositionTracking(order);
    }
}

void TradingEngine::handleOrderRouting(const Order& order) {
    std::cout << "Routing order: " << order.symbol << " qty=" << order.quantity
              << " price=" << order.price << "\n";
    // Placeholder: integrate with venue/exchange adapters here.
}

void TradingEngine::updatePositionTracking(const Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    positions_[order.symbol] += order.quantity;
}

bool TradingEngine::applyRiskChecks(const Order& order) const {
    std::lock_guard<std::mutex> lock(mutex_);

    double currentPosition = 0.0;
    auto positionIt = positions_.find(order.symbol);
    if (positionIt != positions_.end()) {
        currentPosition = positionIt->second;
    }

    double projectedPosition = currentPosition + order.quantity;
    if (riskLimits_.maxPosition > 0.0 && std::abs(projectedPosition) > riskLimits_.maxPosition) {
        std::cout << "Max position exceeded for symbol " << order.symbol << "\n";
        return false;
    }

    double projectedExposure = std::abs(projectedPosition);
    for (const auto& [symbol, qty] : positions_) {
        if (symbol == order.symbol) {
            continue;
        }
        projectedExposure += std::abs(qty);
    }

    if (riskLimits_.maxExposure > 0.0 && projectedExposure > riskLimits_.maxExposure) {
        std::cout << "Max exposure exceeded after order for symbol " << order.symbol << "\n";
        return false;
    }

    return true;
}

void TradingEngine::evaluateAggregateRisk() const {
    std::lock_guard<std::mutex> lock(mutex_);

    double totalExposure = 0.0;
    for (const auto& [symbol, qty] : positions_) {
        totalExposure += std::abs(qty);
        if (riskLimits_.maxPosition > 0.0 && std::abs(qty) > riskLimits_.maxPosition) {
            std::cout << "Warning: position limit breached for symbol " << symbol << "\n";
        }
    }

    if (riskLimits_.maxExposure > 0.0 && totalExposure > riskLimits_.maxExposure) {
        std::cout << "Warning: aggregate exposure limit breached\n";
    }
}

}  // namespace trading
