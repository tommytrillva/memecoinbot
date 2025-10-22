#include "telegram/bot.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "common/logging.h"

namespace telegram {
namespace {
constexpr const char* kHelpText =
    "Available commands:\n"
    "/start - Subscribe to trading updates\n"
    "/help - Show this message\n"
    "/buy <symbol> <quantity> [limit_price] <otp> - Execute a buy order\n"
    "/sell <symbol> <quantity> [limit_price] <otp> - Execute a sell order\n"
    "/status [symbol] - Get the latest portfolio or symbol status";
}  // namespace

TelegramBot::TelegramBot(const std::string& token,
                         trading::TradingEngine& engine,
                         TelegramClient& client)
    : bot_(token), engine_(engine), client_(client),
      aliveFlag_(std::make_shared<std::atomic<bool>>(true)) {
    registerEventHandlers();
    registerEngineCallbacks();

    client_.set_trade_executor([this](const telegram::TradeRequest& trade) {
        ChatId chat_id = 0;
        try {
            chat_id = static_cast<ChatId>(std::stoll(trade.chat_id));
        } catch (const std::exception& ex) {
            LOG_ERROR(std::string("Unable to parse chat id for trade execution: ") + ex.what());
            return;
        }

        trading::OrderRequest request;
        request.symbol = trade.symbol;
        request.quantity = trade.amount;
        request.limitPrice = trade.limit_price;

        trading::OrderReceipt receipt;
        if (trade.side == "sell") {
            receipt = engine_.sell(request);
        } else {
            receipt = engine_.buy(request);
        }

        enqueueMessage(chat_id, formatReceipt(receipt));
    });
}

TelegramBot::~TelegramBot() {
    if (aliveFlag_) {
        aliveFlag_->store(false);
    }
    stop();
}

TgBot::Bot& TelegramBot::bot() { return bot_; }

const TgBot::Bot& TelegramBot::bot() const { return bot_; }

void TelegramBot::start() {
    if (running_.exchange(true)) {
        return;
    }

    dispatchThread_ = std::thread(&TelegramBot::dispatchLoop, this);

    longPollThread_ = std::thread([this]() {
        auto poll = std::make_shared<TgBot::TgLongPoll>(bot_);
        {
            std::lock_guard<std::mutex> lock(longPollMutex_);
            longPoll_ = poll;
        }

        while (running_.load()) {
            try {
                poll->start();
            } catch (const std::exception& ex) {
                if (!running_.load()) {
                    break;
                }
                LOG_WARN(std::string("Telegram long poll error: ") + ex.what());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        {
            std::lock_guard<std::mutex> lock(longPollMutex_);
            longPoll_.reset();
        }
    });
}

void TelegramBot::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(longPollMutex_);
        if (longPoll_) {
            longPoll_->stop();
        }
    }

    queueCondition_.notify_all();

    if (longPollThread_.joinable()) {
        longPollThread_.join();
    }
    if (dispatchThread_.joinable()) {
        dispatchThread_.join();
    }
}

void TelegramBot::registerEventHandlers() {
    bot_.getEvents().onCommand("start", [this](const TgBot::Message::Ptr& message) { handleStart(message); });
    bot_.getEvents().onCommand("help", [this](const TgBot::Message::Ptr& message) { handleHelp(message); });
    bot_.getEvents().onCommand("buy", [this](const TgBot::Message::Ptr& message) { handleBuy(message); });
    bot_.getEvents().onCommand("sell", [this](const TgBot::Message::Ptr& message) { handleSell(message); });
    bot_.getEvents().onCommand("status", [this](const TgBot::Message::Ptr& message) { handleStatus(message); });

    bot_.getEvents().onAnyMessage([this](const TgBot::Message::Ptr& message) {
        if (!message || !message->text) {
            return;
        }
        const std::string text = message->text;
        if (!text.empty() && text.front() == '/') {
            return;  // Command handled elsewhere.
        }
        handlePlainText(message);
    });

    bot_.getEvents().onUnknownCommand([this](const TgBot::Message::Ptr& message) { handleUnknown(message); });
}

void TelegramBot::registerEngineCallbacks() {
    auto alive = aliveFlag_;

    engine_.subscribeToTradeUpdates([this, alive](const trading::TradeUpdate& update) {
        if (!alive || !alive->load()) {
            return;
        }

        std::ostringstream oss;
        oss << (update.success ? "✅" : "⚠️") << " Trade update";
        if (!update.orderId.empty()) {
            oss << " (" << update.orderId << ")";
        }
        if (!update.message.empty()) {
            oss << "\n" << update.message;
        }

        broadcastToSubscribers(oss.str());
    });

    engine_.subscribeToAlerts([this, alive](const trading::AlertUpdate& alert) {
        if (!alive || !alive->load()) {
            return;
        }

        std::ostringstream oss;
        oss << "🚨 " << alert.title;
        if (!alert.body.empty()) {
            oss << "\n" << alert.body;
        }
        broadcastToSubscribers(oss.str());
    });

    engine_.subscribeToStatusUpdates([this, alive](const trading::StatusReport& status) {
        if (!alive || !alive->load()) {
            return;
        }
        broadcastToSubscribers(formatStatus(status));
    });
}

void TelegramBot::handleStart(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat) {
        return;
    }
    rememberSubscriber(message->chat->id);
    enqueueMessage(message->chat->id,
                   "Welcome to the trading bot!\nUse /help to discover available commands.");
}

void TelegramBot::handleHelp(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat) {
        return;
    }
    rememberSubscriber(message->chat->id);
    enqueueMessage(message->chat->id, kHelpText);
}

void TelegramBot::handleBuy(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat || !message->text) {
        return;
    }

    rememberSubscriber(message->chat->id);

    const auto tokens = tokenize(message->text);
    try {
        const auto parsed = parseTradeCommand(tokens);
        telegram::TradeRequest trade;
        trade.chat_id = std::to_string(message->chat->id);
        trade.symbol = parsed.order.symbol;
        trade.amount = parsed.order.quantity;
        trade.limit_price = parsed.order.limitPrice;
        trade.side = "buy";
        trade.otp_code = parsed.otp;

        client_.handle_trade_request(trade);
    } catch (const std::exception& ex) {
        enqueueMessage(message->chat->id,
                       std::string("❌ Unable to execute buy order: ") + ex.what());
    }
}

void TelegramBot::handleSell(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat || !message->text) {
        return;
    }

    rememberSubscriber(message->chat->id);

    const auto tokens = tokenize(message->text);
    try {
        const auto parsed = parseTradeCommand(tokens);
        telegram::TradeRequest trade;
        trade.chat_id = std::to_string(message->chat->id);
        trade.symbol = parsed.order.symbol;
        trade.amount = parsed.order.quantity;
        trade.limit_price = parsed.order.limitPrice;
        trade.side = "sell";
        trade.otp_code = parsed.otp;

        client_.handle_trade_request(trade);
    } catch (const std::exception& ex) {
        enqueueMessage(message->chat->id,
                       std::string("❌ Unable to execute sell order: ") + ex.what());
    }
}

void TelegramBot::handleStatus(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat || !message->text) {
        return;
    }

    rememberSubscriber(message->chat->id);

    const auto tokens = tokenize(message->text);
    std::optional<std::string> symbol;
    if (tokens.size() >= 2) {
        symbol = tokens[1];
    }

    try {
        const auto report = engine_.status(symbol);
        enqueueMessage(message->chat->id, formatStatus(report));
    } catch (const std::exception& ex) {
        enqueueMessage(message->chat->id,
                       std::string("❌ Unable to retrieve status: ") + ex.what());
    }
}

void TelegramBot::handleUnknown(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat) {
        return;
    }

    enqueueMessage(message->chat->id,
                   "Unrecognized command. Use /help to view available options.");
}

void TelegramBot::handlePlainText(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat) {
        return;
    }

    rememberSubscriber(message->chat->id);
    enqueueMessage(message->chat->id,
                   "Hi there! Use /help to discover supported commands.");
}

std::vector<std::string> TelegramBot::tokenize(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::optional<double> TelegramBot::parseDouble(const std::string& token) {
    try {
        size_t processed = 0;
        const double value = std::stod(token, &processed);
        if (processed != token.size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

TelegramBot::ParsedTradeCommand TelegramBot::parseTradeCommand(const std::vector<std::string>& tokens) const {
    if (tokens.size() < 4 || tokens.size() > 5) {
        throw std::invalid_argument("Command requires <symbol> <quantity> [limit_price] <otp>.");
    }

    ParsedTradeCommand parsed;
    parsed.order.symbol = tokens[1];

    const auto quantity = parseDouble(tokens[2]);
    if (!quantity || *quantity <= 0.0) {
        throw std::invalid_argument("Quantity must be a positive number.");
    }
    parsed.order.quantity = *quantity;

    const std::size_t otp_index = tokens.size() == 5 ? 4 : 3;
    if (otp_index == 4) {
        const auto price = parseDouble(tokens[3]);
        if (!price || *price <= 0.0) {
            throw std::invalid_argument("Limit price must be a positive number.");
        }
        parsed.order.limitPrice = price;
    }

    const std::string& otp = tokens[otp_index];
    if (otp.size() < 6 || otp.size() > 8 ||
        !std::all_of(otp.begin(), otp.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
        throw std::invalid_argument("OTP must be a 6-8 digit numeric code.");
    }
    parsed.otp = otp;

    return parsed;
}

std::string TelegramBot::formatReceipt(const trading::OrderReceipt& receipt) const {
    std::ostringstream oss;
    oss << (receipt.success ? "✅" : "❌") << ' ' << receipt.message;
    if (!receipt.orderId.empty()) {
        oss << "\nOrder ID: " << receipt.orderId;
    }
    if (receipt.filledQuantity > 0.0) {
        oss << "\nFilled: " << std::fixed << std::setprecision(4) << receipt.filledQuantity;
    }
    if (receipt.averagePrice > 0.0) {
        oss << " @ " << std::fixed << std::setprecision(4) << receipt.averagePrice;
    }
    return oss.str();
}

std::string TelegramBot::formatStatus(const trading::StatusReport& status) const {
    std::ostringstream oss;
    oss << "📊 " << (status.summary.empty() ? "Portfolio status" : status.summary);
    if (!status.positions.empty()) {
        oss << "\n";
        for (const auto& position : status.positions) {
            oss << "• " << position << "\n";
        }
    }
    return oss.str();
}

void TelegramBot::enqueueMessage(ChatId chatId, std::string text) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        messageQueue_.emplace(chatId, std::move(text));
    }
    queueCondition_.notify_one();
}

void TelegramBot::broadcastToSubscribers(const std::string& text) {
    std::vector<ChatId> recipients;
    {
        std::lock_guard<std::mutex> lock(subscribersMutex_);
        recipients.assign(subscribers_.begin(), subscribers_.end());
    }

    for (const auto chatId : recipients) {
        enqueueMessage(chatId, text);
    }
}

void TelegramBot::dispatchLoop() {
    while (running_.load() || !messageQueue_.empty()) {
        MessagePayload payload;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCondition_.wait(lock, [this]() {
                return !messageQueue_.empty() || !running_.load();
            });

            if (messageQueue_.empty()) {
                continue;
            }

            payload = std::move(messageQueue_.front());
            messageQueue_.pop();
        }

        try {
            bot_.getApi().sendMessage(payload.first, payload.second);
        } catch (const std::exception& ex) {
            LOG_WARN(std::string("Failed to send Telegram message: ") + ex.what());
        }
    }
}

void TelegramBot::rememberSubscriber(ChatId chatId) {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    subscribers_.insert(chatId);
}

}  // namespace telegram
