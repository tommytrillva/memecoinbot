#include "ui/data_subscription.h"

#include <utility>
#include <vector>

namespace ui {

SubscriptionToken::SubscriptionToken(std::function<void()> unsubscribe_fn)
    : m_unsubscribe(std::move(unsubscribe_fn)) {}

SubscriptionToken::~SubscriptionToken() { reset(); }

SubscriptionToken::SubscriptionToken(SubscriptionToken &&other) noexcept
    : m_unsubscribe(std::move(other.m_unsubscribe)) {}

SubscriptionToken &SubscriptionToken::operator=(SubscriptionToken &&other) noexcept {
    if (this != &other) {
        reset();
        m_unsubscribe = std::move(other.m_unsubscribe);
    }
    return *this;
}

void SubscriptionToken::reset() {
    if (m_unsubscribe) {
        auto unsubscribe = std::move(m_unsubscribe);
        unsubscribe();
    }
}

SubscriptionToken MarketDataBus::subscribe_price(const std::string &symbol, PriceCallback callback) {
    std::lock_guard lock(m_mutex);
    const std::size_t subscription_id = m_next_subscription_id++;
    auto &symbol_subscribers = m_price_subscribers[symbol];
    symbol_subscribers.callbacks.emplace(subscription_id, std::move(callback));

    return SubscriptionToken([this, symbol, subscription_id]() {
        std::lock_guard inner_lock(m_mutex);
        auto it = m_price_subscribers.find(symbol);
        if (it != m_price_subscribers.end()) {
            it->second.callbacks.erase(subscription_id);
            if (it->second.callbacks.empty()) {
                m_price_subscribers.erase(it);
            }
        }
    });
}

void MarketDataBus::publish_price(const PricePoint &point) {
    std::vector<PriceCallback> callbacks;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_price_subscribers.find(point.symbol);
        if (it != m_price_subscribers.end()) {
            callbacks.reserve(it->second.callbacks.size());
            for (const auto &entry : it->second.callbacks) {
                callbacks.push_back(entry.second);
            }
        }
    }

    for (auto &callback : callbacks) {
        if (callback) {
            callback(point);
        }
    }
}

SubscriptionToken EngineEventBus::subscribe_position(PositionCallback callback) {
    std::lock_guard lock(m_mutex);
    const std::size_t subscription_id = m_next_position_id++;
    m_position_subscribers.emplace(subscription_id, std::move(callback));

    return SubscriptionToken([this, subscription_id]() {
        std::lock_guard inner_lock(m_mutex);
        m_position_subscribers.erase(subscription_id);
    });
}

SubscriptionToken EngineEventBus::subscribe_trade(TradeCallback callback) {
    std::lock_guard lock(m_mutex);
    const std::size_t subscription_id = m_next_trade_id++;
    m_trade_subscribers.emplace(subscription_id, std::move(callback));

    return SubscriptionToken([this, subscription_id]() {
        std::lock_guard inner_lock(m_mutex);
        m_trade_subscribers.erase(subscription_id);
    });
}

void EngineEventBus::publish_position(const PositionUpdate &update) {
    std::vector<PositionCallback> callbacks;
    {
        std::lock_guard lock(m_mutex);
        callbacks.reserve(m_position_subscribers.size());
        for (const auto &entry : m_position_subscribers) {
            callbacks.push_back(entry.second);
        }
    }

    for (auto &callback : callbacks) {
        if (callback) {
            callback(update);
        }
    }
}

void EngineEventBus::publish_trade(const TradeEvent &event) {
    std::vector<TradeCallback> callbacks;
    {
        std::lock_guard lock(m_mutex);
        callbacks.reserve(m_trade_subscribers.size());
        for (const auto &entry : m_trade_subscribers) {
            callbacks.push_back(entry.second);
        }
    }

    for (auto &callback : callbacks) {
        if (callback) {
            callback(event);
        }
    }
}

} // namespace ui
