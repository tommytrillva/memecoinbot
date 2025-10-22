#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "../security/secret_store.h"
#include "../security/totp.h"

namespace telegram {

struct TradeRequest {
    std::string chat_id;
    std::string symbol;
    double amount = 0.0;
    std::string side;
    std::string otp_code;
    std::optional<double> limit_price;
};

// TelegramClient receives updates from the Telegram gateway and performs
// 2FA validation before handing trades over to the execution layer.
class TelegramClient {
  public:
    TelegramClient(std::shared_ptr<security::SecretStore> secret_store,
                   std::shared_ptr<security::TotpValidator> totp_validator);

    // Registers the downstream trade executor. The callback is only invoked
    // when a request passes TOTP verification.
    void set_trade_executor(std::function<void(const TradeRequest &)> executor);

    // Processes a high-level trade request derived from an incoming Telegram
    // update. Validation failures raise std::runtime_error so callers can
    // provide human readable feedback to operators.
    void handle_trade_request(const TradeRequest &request);

  private:
    void ensure_executor() const;
    std::string secret_key_for_chat(const std::string &chat_id) const;
    bool verify_totp(const std::string &chat_id, const std::string &code) const;

    std::shared_ptr<security::SecretStore> secret_store_;
    std::shared_ptr<security::TotpValidator> totp_validator_;
    std::function<void(const TradeRequest &)> trade_executor_;
};

}  // namespace telegram
