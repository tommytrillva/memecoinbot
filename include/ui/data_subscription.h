#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ui {

struct PricePoint {
    std::string symbol;
    double price = 0.0;
    double volume = 0.0;
    std::chrono::system_clock::time_point timestamp{};
};

struct PositionUpdate {
    std::string position_id;
    std::string symbol;
    double quantity = 0.0;
    double entry_price = 0.0;
    double mark_price = 0.0;
    double unrealized_pnl = 0.0;
    std::chrono::system_clock::time_point timestamp{};
};

struct TradeEvent {
    std::string trade_id;
    std::string symbol;
    double quantity = 0.0;
    double price = 0.0;
    bool is_buy = true;
    std::chrono::system_clock::time_point timestamp{};
};

class SubscriptionToken {
  public:
    SubscriptionToken() = default;
    explicit SubscriptionToken(std::function<void()> unsubscribe_fn);
    ~SubscriptionToken();

    SubscriptionToken(const SubscriptionToken &) = delete;
    SubscriptionToken &operator=(const SubscriptionToken &) = delete;

    SubscriptionToken(SubscriptionToken &&other) noexcept;
    SubscriptionToken &operator=(SubscriptionToken &&other) noexcept;

    void reset();
    explicit operator bool() const noexcept { return static_cast<bool>(m_unsubscribe); }

  private:
    std::function<void()> m_unsubscribe{};
};

class MarketDataBus {
  public:
    using PriceCallback = std::function<void(const PricePoint &)>;

    SubscriptionToken subscribe_price(const std::string &symbol, PriceCallback callback);
    void publish_price(const PricePoint &point);

  private:
    struct SymbolSubscriptions {
        std::unordered_map<std::size_t, PriceCallback> callbacks;
    };

    std::mutex m_mutex;
    std::unordered_map<std::string, SymbolSubscriptions> m_price_subscribers;
    std::size_t m_next_subscription_id = 1;
};

class EngineEventBus {
  public:
    using PositionCallback = std::function<void(const PositionUpdate &)>;
    using TradeCallback = std::function<void(const TradeEvent &)>;

    SubscriptionToken subscribe_position(PositionCallback callback);
    SubscriptionToken subscribe_trade(TradeCallback callback);

    void publish_position(const PositionUpdate &update);
    void publish_trade(const TradeEvent &event);

  private:
    std::mutex m_mutex;
    std::unordered_map<std::size_t, PositionCallback> m_position_subscribers;
    std::unordered_map<std::size_t, TradeCallback> m_trade_subscribers;
    std::size_t m_next_position_id = 1;
    std::size_t m_next_trade_id = 1;
};

} // namespace ui
