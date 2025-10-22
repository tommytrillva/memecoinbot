#include "telegram_client.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace telegram {

TelegramClient::TelegramClient(std::shared_ptr<security::SecretStore> secret_store,
                               std::shared_ptr<security::TotpValidator> totp_validator)
    : secret_store_(std::move(secret_store)), totp_validator_(std::move(totp_validator)) {
    if (!secret_store_) {
        throw std::invalid_argument("SecretStore dependency is required");
    }
    if (!totp_validator_) {
        throw std::invalid_argument("TotpValidator dependency is required");
    }
}

void TelegramClient::set_trade_executor(std::function<void(const TradeRequest &)> executor) {
    trade_executor_ = std::move(executor);
}

void TelegramClient::handle_trade_request(const TradeRequest &request) {
    ensure_executor();

    if (request.chat_id.empty()) {
        throw std::runtime_error("Missing chat identifier in trade request");
    }
    if (request.otp_code.empty()) {
        throw std::runtime_error("Two-factor code is required before executing trades");
    }

    if (!verify_totp(request.chat_id, request.otp_code)) {
        throw std::runtime_error("The supplied two-factor code is invalid or expired");
    }

    trade_executor_(request);
}

void TelegramClient::ensure_executor() const {
    if (!trade_executor_) {
        throw std::logic_error("No trade executor configured for TelegramClient");
    }
}

std::string TelegramClient::secret_key_for_chat(const std::string &chat_id) const {
    constexpr char kPrefix[] = "telegram/totp/";
    return std::string(kPrefix) + chat_id;
}

bool TelegramClient::verify_totp(const std::string &chat_id, const std::string &code) const {
    const auto key_name = secret_key_for_chat(chat_id);
    const auto totp_secret = secret_store_->get_secret(key_name);
    if (!totp_secret.has_value()) {
        std::ostringstream message;
        message << "No registered TOTP secret for chat " << chat_id
                << ". A secret must be provisioned in the encrypted secret store.";
        throw std::runtime_error(message.str());
    }

    return totp_validator_->validate(*totp_secret, code);
}

}  // namespace telegram
