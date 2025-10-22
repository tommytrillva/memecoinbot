#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace trading {

struct OrderRequest {
    std::string symbol;
    double quantity{0.0};
    std::optional<double> limitPrice;
};

struct OrderReceipt {
    bool success{false};
    std::string message;
    std::string orderId;
    double filledQuantity{0.0};
    double averagePrice{0.0};
};

struct StatusReport {
    std::string summary;
    std::vector<std::string> positions;
};

struct TradeUpdate {
    std::string orderId;
    std::string message;
    bool success{false};
};

struct AlertUpdate {
    std::string title;
    std::string body;
};

class TradingEngine {
public:
    using TradeCallback = std::function<void(const TradeUpdate&)>;
    using AlertCallback = std::function<void(const AlertUpdate&)>;
    using StatusCallback = std::function<void(const StatusReport&)>;

    virtual ~TradingEngine() = default;

    virtual OrderReceipt buy(const OrderRequest& request) = 0;
    virtual OrderReceipt sell(const OrderRequest& request) = 0;
    virtual StatusReport status(const std::optional<std::string>& symbol) const = 0;

    virtual void subscribeToTradeUpdates(TradeCallback callback) = 0;
    virtual void subscribeToAlerts(AlertCallback callback) = 0;
    virtual void subscribeToStatusUpdates(StatusCallback callback) = 0;
};

}  // namespace trading
