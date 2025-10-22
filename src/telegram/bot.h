#pragma once

#include <tgbot/tgbot.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "trading/trading_engine.h"
#include "telegram/telegram_client.h"

namespace telegram {

class TelegramBot {
public:
    using ChatId = std::int64_t;

    TelegramBot(const std::string& token,
                trading::TradingEngine& engine,
                TelegramClient& client);
    ~TelegramBot();

    TelegramBot(const TelegramBot&) = delete;
    TelegramBot& operator=(const TelegramBot&) = delete;

    void start();
    void stop();

    TgBot::Bot& bot();
    const TgBot::Bot& bot() const;

private:
    using MessagePayload = std::pair<ChatId, std::string>;

    void registerEventHandlers();
    void registerEngineCallbacks();

    void handleStart(const TgBot::Message::Ptr& message);
    void handleHelp(const TgBot::Message::Ptr& message);
    void handleBuy(const TgBot::Message::Ptr& message);
    void handleSell(const TgBot::Message::Ptr& message);
    void handleStatus(const TgBot::Message::Ptr& message);
    void handleUnknown(const TgBot::Message::Ptr& message);
    void handlePlainText(const TgBot::Message::Ptr& message);

    static std::vector<std::string> tokenize(const std::string& text);
    static std::optional<double> parseDouble(const std::string& token);

    struct ParsedTradeCommand {
        trading::OrderRequest order;
        std::string otp;
    };

    ParsedTradeCommand parseTradeCommand(const std::vector<std::string>& tokens) const;
    std::string formatReceipt(const trading::OrderReceipt& receipt) const;
    std::string formatStatus(const trading::StatusReport& status) const;

    void enqueueMessage(ChatId chatId, std::string text);
    void broadcastToSubscribers(const std::string& text);
    void dispatchLoop();

    void rememberSubscriber(ChatId chatId);

    TgBot::Bot bot_;
    trading::TradingEngine& engine_;
    TelegramClient& client_;

    std::shared_ptr<std::atomic<bool>> aliveFlag_;
    std::atomic<bool> running_{false};

    std::thread longPollThread_;
    std::thread dispatchThread_;

    std::shared_ptr<TgBot::TgLongPoll> longPoll_;
    std::mutex longPollMutex_;

    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::queue<MessagePayload> messageQueue_;

    std::mutex subscribersMutex_;
    std::set<ChatId> subscribers_;
};

}  // namespace telegram
