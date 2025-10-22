#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace market_data {

class PumpFunClientTestPeer;

// Represents token level metadata returned by Pump.fun indexers.
struct TokenMetadata {
  std::string mint;
  std::string name;
  std::string symbol;
  std::string description;
  std::string image_url;
  double market_cap = 0.0;
  double liquidity = 0.0;
  std::uint64_t holder_count = 0;
  std::string last_updated;
};

// Represents a real-time quote for a Pump.fun token.
struct TokenQuote {
  std::string mint;
  double price = 0.0;
  double price_change_24h = 0.0;
  double volume_24h = 0.0;
  double liquidity = 0.0;
  std::string timestamp;
};

// Represents a historical OHLCV candle for a Pump.fun token.
struct HistoricalCandle {
  std::string mint;
  std::string timeframe;
  std::string open_time;
  std::string close_time;
  double open = 0.0;
  double high = 0.0;
  double low = 0.0;
  double close = 0.0;
  double volume = 0.0;
  double quote_volume = 0.0;
};

// PumpFunClient encapsulates HTTP calls to Moralis/QuickNode Pump.fun endpoints and exposes
// synchronous fetch APIs as well as a lightweight polling/streaming interface that the
// trading engine can subscribe to for real-time updates.
class PumpFunClient {
 public:
  using SubscriptionId = std::uint64_t;
  using QuoteCallback = std::function<void(const TokenQuote&)>;
  using HttpGetFunction = std::function<std::string(
      const std::string& endpoint,
      const std::vector<std::pair<std::string, std::string>>& query_params,
      const std::unordered_map<std::string, std::string>& extra_headers)>;

  struct RequestOptions {
    std::vector<std::pair<std::string, std::string>> query_params;
    std::unordered_map<std::string, std::string> headers;
  };

  PumpFunClient(std::string base_url,
                std::string api_key = {},
                std::string metadata_endpoint = "/metadata",
                std::string quote_endpoint = "/quotes",
                std::string candles_endpoint = "/candles",
                HttpGetFunction http_getter = {});
  ~PumpFunClient();

  PumpFunClient(const PumpFunClient&) = delete;
  PumpFunClient& operator=(const PumpFunClient&) = delete;
  PumpFunClient(PumpFunClient&&) = delete;
  PumpFunClient& operator=(PumpFunClient&&) = delete;

  // Fetches token metadata for a given mint address.
  TokenMetadata fetchTokenMetadata(
      const std::string& token_mint,
      const std::unordered_map<std::string, std::string>& extra_headers = {}) const;

  // Fetches a single real-time quote for the given mint address.
  TokenQuote fetchTokenQuote(
      const std::string& token_mint,
      const std::unordered_map<std::string, std::string>& extra_headers = {}) const;

  // Fetches a window of historical OHLCV candles for the provided mint and timeframe.
  std::vector<HistoricalCandle> fetchHistoricalCandles(
      const std::string& token_mint,
      const std::string& timeframe,
      int limit = 100,
      const std::unordered_map<std::string, std::string>& extra_headers = {}) const;

  // Registers a polling subscription that periodically pulls quotes and invokes the
  // callback on the calling thread. Returns a handle that can be used to unsubscribe.
  SubscriptionId subscribeToQuotes(const std::string& token_mint,
                                   QuoteCallback callback,
                                   std::chrono::milliseconds interval = std::chrono::milliseconds(1500));

  // Stops and removes a subscription by id.
  void unsubscribe(SubscriptionId id);

  // Returns true if the specified subscription has encountered a callback error.
  bool subscriptionHadCallbackError(SubscriptionId id) const;

  // Stops all outstanding polling subscriptions (e.g., when shutting down the engine).
  void stopAll();

  // Allows callers to augment default headers (e.g., Authorization, API keys).
  void setDefaultHeaders(std::unordered_map<std::string, std::string> headers);
  std::unordered_map<std::string, std::string> defaultHeaders() const;

  void setRetryPolicy(std::size_t max_attempts,
                      std::chrono::milliseconds initial_backoff);

 private:
  friend class PumpFunClientTestPeer;

  struct Subscription {
    std::string token_mint;
    QuoteCallback callback;
    std::chrono::milliseconds interval;
    std::atomic<bool> active{true};
    std::atomic<bool> callback_error{false};
    std::thread worker;
  };

  TokenMetadata parseTokenMetadata(const nlohmann::json& json) const;
  TokenQuote parseTokenQuote(const nlohmann::json& json) const;
  HistoricalCandle parseHistoricalCandle(const nlohmann::json& json,
                                         const std::string& token_mint,
                                         const std::string& timeframe) const;

  std::string buildUrl(const std::string& endpoint,
                       const std::vector<std::pair<std::string, std::string>>& query_params) const;
  std::string performGet(const std::string& endpoint,
                         const std::vector<std::pair<std::string, std::string>>& query_params,
                         const std::unordered_map<std::string, std::string>& extra_headers) const;
  std::string performCurlGet(const std::string& endpoint,
                             const std::vector<std::pair<std::string, std::string>>& query_params,
                             const std::unordered_map<std::string, std::string>& extra_headers) const;
  static std::string encodeQueryParam(const std::string& value);

  static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
  void drainSubscriptions();

  std::string base_url_;
  std::string api_key_;
  std::string metadata_endpoint_;
  std::string quote_endpoint_;
  std::string candles_endpoint_;
  std::unordered_map<std::string, std::string> default_headers_;

  HttpGetFunction http_getter_;

  std::atomic<std::size_t> max_attempts_{3};
  std::atomic<long long> retry_backoff_ms_{200};

  bool curl_initialized_ = false;

  mutable std::mutex http_mutex_;

  std::atomic<bool> running_{true};
  std::atomic<SubscriptionId> next_subscription_id_{1};
  mutable std::mutex subscriptions_mutex_;
  std::unordered_map<SubscriptionId, std::shared_ptr<Subscription>> subscriptions_;
};

}  // namespace market_data

