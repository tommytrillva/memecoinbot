#include "trading/engine.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace trading {
namespace {
constexpr auto kEngineSleepInterval = std::chrono::milliseconds(100);
}

RiskManagedEngine::RiskManagedEngine() : RiskManagedEngine(RiskLimits{}) {}

RiskManagedEngine::RiskManagedEngine(RiskLimits limits) : riskLimits_(std::move(limits)) {}

RiskManagedEngine::~RiskManagedEngine() {
    stop();
}

void RiskManagedEngine::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_ = std::thread(&RiskManagedEngine::executionLoop, this);
}

void RiskManagedEngine::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

bool RiskManagedEngine::isRunning() const {
    return running_.load();
}

void RiskManagedEngine::updateRiskLimits(const RiskLimits& limits) {
    std::lock_guard<std::mutex> lock(mutex_);
    riskLimits_ = limits;
}

OrderReceipt RiskManagedEngine::buy(const OrderRequest& request) {
    return submitOrder(request, Order::Side::Buy);
}

OrderReceipt RiskManagedEngine::sell(const OrderRequest& request) {
    return submitOrder(request, Order::Side::Sell);
}

StatusReport RiskManagedEngine::status(const std::optional<std::string>& symbol) const {
    return buildStatusReport(symbol);
}

void RiskManagedEngine::subscribeToTradeUpdates(TradeCallback callback) {
    if (!callback) {
        return;
    }
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    tradeSubscribers_.push_back(std::move(callback));
}

void RiskManagedEngine::subscribeToAlerts(AlertCallback callback) {
    if (!callback) {
        return;
    }
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    alertSubscribers_.push_back(std::move(callback));
}

void RiskManagedEngine::subscribeToStatusUpdates(StatusCallback callback) {
    if (!callback) {
        return;
    }
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    statusSubscribers_.push_back(std::move(callback));
}

OrderReceipt RiskManagedEngine::submitOrder(const OrderRequest& request, Order::Side side) {
    OrderReceipt receipt;

    if (!isRunning()) {
        receipt.success = false;
        receipt.message = "Engine is not running; unable to accept orders.";
        return receipt;
    }

    if (request.symbol.empty()) {
        receipt.success = false;
        receipt.message = "Symbol must be specified.";
        return receipt;
    }

    if (request.quantity <= 0.0) {
        receipt.success = false;
        receipt.message = "Quantity must be greater than zero.";
        return receipt;
    }

    Order order;
    order.orderId = generateOrderId();
    order.symbol = request.symbol;
    order.quantity = request.quantity;
    order.limitPrice = request.limitPrice;
    order.side = side;

    if (!applyRiskChecks(order)) {
        TradeUpdate update;
        update.orderId = order.orderId;
        update.success = false;
        update.message = "Risk controls rejected order for symbol " + order.symbol;
        notifyTradeUpdate(update);

        receipt.success = false;
        receipt.message = update.message;
        receipt.orderId = order.orderId;
        return receipt;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        orderQueue_.push_back(order);
    }

    TradeUpdate acceptance;
    acceptance.orderId = order.orderId;
    acceptance.success = true;
    {
        std::ostringstream oss;
        oss << "Accepted order for "
            << (order.side == Order::Side::Buy ? "buy" : "sell")
            << " " << order.quantity << " of " << order.symbol;
        if (order.limitPrice) {
            oss << " @ " << *order.limitPrice;
        }
        acceptance.message = oss.str();
    }
    notifyTradeUpdate(acceptance);

    receipt.success = true;
    receipt.message = "Order queued for execution.";
    receipt.orderId = order.orderId;
    receipt.averagePrice = order.limitPrice.value_or(0.0);
    receipt.filledQuantity = 0.0;
    return receipt;
}

void RiskManagedEngine::executionLoop() {
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

    std::vector<Order> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(orderQueue_);
    }
    if (!pending.empty()) {
        routePendingOrders(pending);
    }
}

void RiskManagedEngine::routePendingOrders(std::vector<Order>& orders) {
    for (const auto& order : orders) {
        if (!applyRiskChecks(order)) {
            TradeUpdate update;
            update.orderId = order.orderId;
            update.success = false;
            update.message = "Risk control rejected order for symbol " + order.symbol;
            notifyTradeUpdate(update);
            continue;
        }

        handleOrderRouting(order);
        updatePositionTracking(order);

        TradeUpdate update;
        update.orderId = order.orderId;
        update.success = true;
        {
            std::ostringstream oss;
            oss << "Executed "
                << (order.side == Order::Side::Buy ? "buy" : "sell")
                << " order for " << order.symbol << " (" << order.quantity << ")";
            if (order.limitPrice) {
                oss << " @ " << *order.limitPrice;
            }
            update.message = oss.str();
        }
        notifyTradeUpdate(update);

        notifyStatusUpdate(status(std::nullopt));
    }
}

void RiskManagedEngine::handleOrderRouting(const Order& order) {
    std::cout << "Routing order: "
              << (order.side == Order::Side::Buy ? "BUY " : "SELL ")
              << order.symbol << " qty=" << order.quantity;
    if (order.limitPrice) {
        std::cout << " price=" << *order.limitPrice;
    }
    std::cout << "\n";
    // Placeholder: integrate with venue/exchange adapters here.
}

void RiskManagedEngine::updatePositionTracking(const Order& order) {
    const double signedQuantity =
        order.side == Order::Side::Buy ? order.quantity : -order.quantity;

    std::lock_guard<std::mutex> lock(mutex_);
    positions_[order.symbol] += signedQuantity;
}

bool RiskManagedEngine::applyRiskChecks(const Order& order) const {
    std::lock_guard<std::mutex> lock(mutex_);

    double currentPosition = 0.0;
    auto positionIt = positions_.find(order.symbol);
    if (positionIt != positions_.end()) {
        currentPosition = positionIt->second;
    }

    const double signedQuantity =
        order.side == Order::Side::Buy ? order.quantity : -order.quantity;
    double projectedPosition = currentPosition + signedQuantity;
    if (riskLimits_.maxPosition > 0.0 &&
        std::abs(projectedPosition) > riskLimits_.maxPosition) {
        return false;
    }

    double projectedExposure = std::abs(projectedPosition);
    for (const auto& [symbol, qty] : positions_) {
        if (symbol == order.symbol) {
            continue;
        }
        projectedExposure += std::abs(qty);
    }

    if (riskLimits_.maxExposure > 0.0 &&
        projectedExposure > riskLimits_.maxExposure) {
        return false;
    }

    return true;
}

void RiskManagedEngine::evaluateAggregateRisk() const {
    std::vector<std::string> warnings;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        double totalExposure = 0.0;
        for (const auto& [symbol, qty] : positions_) {
            const double absoluteQty = std::abs(qty);
            totalExposure += absoluteQty;
            if (riskLimits_.maxPosition > 0.0 &&
                absoluteQty > riskLimits_.maxPosition) {
                std::ostringstream oss;
                oss << "Position limit breached for symbol " << symbol
                    << " (" << absoluteQty << ")";
                warnings.push_back(oss.str());
            }
        }

        if (riskLimits_.maxExposure > 0.0 &&
            totalExposure > riskLimits_.maxExposure) {
            std::ostringstream oss;
            oss << "Aggregate exposure limit breached (" << totalExposure << ")";
            warnings.push_back(oss.str());
        }
    }

    for (const auto& warning : warnings) {
        AlertUpdate alert;
        alert.title = "Risk Warning";
        alert.body = warning;
        notifyAlert(alert);
    }
}

void RiskManagedEngine::notifyTradeUpdate(const TradeUpdate& update) const {
    std::vector<TradeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callbacks = tradeSubscribers_;
    }

    for (const auto& callback : callbacks) {
        if (callback) {
            callback(update);
        }
    }
}

void RiskManagedEngine::notifyAlert(const AlertUpdate& alert) const {
    std::vector<AlertCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callbacks = alertSubscribers_;
    }

    for (const auto& callback : callbacks) {
        if (callback) {
            callback(alert);
        }
    }
}

void RiskManagedEngine::notifyStatusUpdate(const StatusReport& report) const {
    std::vector<StatusCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callbacks = statusSubscribers_;
    }

    for (const auto& callback : callbacks) {
        if (callback) {
            callback(report);
        }
    }
}

std::string RiskManagedEngine::generateOrderId() {
    const auto id = ++orderCounter_;
    return "ORD-" + std::to_string(id);
}

StatusReport RiskManagedEngine::buildStatusReport(const std::optional<std::string>& symbol) const {
    StatusReport report;
    std::lock_guard<std::mutex> lock(mutex_);

    if (symbol) {
        report.summary = "Status for " + *symbol;
        const auto it = positions_.find(*symbol);
        const double qty = it != positions_.end() ? it->second : 0.0;
        std::ostringstream oss;
        oss << *symbol << ": " << std::fixed << std::setprecision(4) << qty;
        report.positions.push_back(oss.str());
    } else {
        report.summary = "Portfolio status";
        if (positions_.empty()) {
            report.positions.push_back("No open positions.");
        } else {
            for (const auto& [sym, qty] : positions_) {
                std::ostringstream oss;
                oss << sym << ": " << std::fixed << std::setprecision(4) << qty;
                report.positions.push_back(oss.str());
            }
        }
    }

    return report;
}

}  // namespace trading
