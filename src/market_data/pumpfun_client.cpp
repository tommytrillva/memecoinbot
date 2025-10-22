#include "market_data/pumpfun_client.h"

#include <curl/curl.h>

#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <thread>

namespace market_data {
namespace {
nlohmann::json parseJsonOrThrow(const std::string& payload, const std::string& context) {
  try {
    if (payload.empty()) {
      return nlohmann::json::object();
    }
    return nlohmann::json::parse(payload);
  } catch (const nlohmann::json::exception& ex) {
    const std::string snippet = payload.size() > 256 ? payload.substr(0, 256) + "..." : payload;
    throw std::runtime_error("Failed to parse " + context +
                             " response: " + std::string(ex.what()) + " (payload snippet: " + snippet + ")");
  }
}

std::string normalizeBaseUrl(std::string url) {
  while (!url.empty() && url.back() == '/') {
    url.pop_back();
  }
  return url;
}

std::string ensureEndpoint(const std::string& endpoint) {
  if (endpoint.empty()) {
    return endpoint;
  }
  if (endpoint.front() != '/') {
    return "/" + endpoint;
  }
  return endpoint;
}
}  // namespace

PumpFunClient::PumpFunClient(std::string base_url,
                             std::string api_key,
                             std::string metadata_endpoint,
                             std::string quote_endpoint,
                             std::string candles_endpoint)
    : base_url_(normalizeBaseUrl(std::move(base_url))),
      api_key_(std::move(api_key)),
      metadata_endpoint_(ensureEndpoint(metadata_endpoint)),
      quote_endpoint_(ensureEndpoint(quote_endpoint)),
      candles_endpoint_(ensureEndpoint(candles_endpoint)) {
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    throw std::runtime_error("Failed to initialize cURL");
  }
  curl_initialized_ = true;

  if (!api_key_.empty()) {
    default_headers_["x-api-key"] = api_key_;
  }
}

PumpFunClient::~PumpFunClient() {
  running_ = false;
  drainSubscriptions();

  if (curl_initialized_) {
    curl_global_cleanup();
    curl_initialized_ = false;
  }
}

void PumpFunClient::setDefaultHeaders(std::unordered_map<std::string, std::string> headers) {
  std::lock_guard<std::mutex> lock(http_mutex_);
  default_headers_ = std::move(headers);
  if (!api_key_.empty() && default_headers_.find("x-api-key") == default_headers_.end()) {
    default_headers_["x-api-key"] = api_key_;
  }
}

std::unordered_map<std::string, std::string> PumpFunClient::defaultHeaders() const {
  std::lock_guard<std::mutex> lock(http_mutex_);
  return default_headers_;
}

TokenMetadata PumpFunClient::fetchTokenMetadata(
    const std::string& token_mint,
    const std::unordered_map<std::string, std::string>& extra_headers) const {
  std::string endpoint = metadata_endpoint_;
  if (!endpoint.empty()) {
    endpoint += "/" + token_mint;
  }

  const std::string response = performGet(endpoint, {}, extra_headers);

  nlohmann::json json = parseJsonOrThrow(response, "token metadata");
  if (json.contains("result")) {
    json = json["result"];
  }
  if (json.contains("data") && json["data"].is_object()) {
    json = json["data"];
  }
  if (json.contains("metadata")) {
    json = json["metadata"];
  }

  return parseTokenMetadata(json);
}

TokenQuote PumpFunClient::fetchTokenQuote(
    const std::string& token_mint,
    const std::unordered_map<std::string, std::string>& extra_headers) const {
  std::string endpoint = quote_endpoint_;
  if (!endpoint.empty()) {
    endpoint += "/" + token_mint;
  }

  const std::string response = performGet(endpoint, {}, extra_headers);

  nlohmann::json json = parseJsonOrThrow(response, "token quote");
  if (json.contains("result")) {
    json = json["result"];
  }
  if (json.contains("data") && json["data"].is_object()) {
    json = json["data"];
  }
  if (json.is_array() && !json.empty()) {
    json = json.front();
  }

  return parseTokenQuote(json);
}

std::vector<HistoricalCandle> PumpFunClient::fetchHistoricalCandles(
    const std::string& token_mint,
    const std::string& timeframe,
    int limit,
    const std::unordered_map<std::string, std::string>& extra_headers) const {
  if (limit <= 0) {
    throw std::invalid_argument("limit must be greater than zero");
  }

  std::vector<std::pair<std::string, std::string>> query_params = {
      {"timeframe", timeframe},
      {"limit", std::to_string(limit)},
  };

  std::string endpoint = candles_endpoint_;
  if (!endpoint.empty()) {
    endpoint += "/" + token_mint;
  }

  const std::string response = performGet(endpoint, query_params, extra_headers);

  nlohmann::json json = parseJsonOrThrow(response, "historical candles");
  if (json.contains("result")) {
    json = json["result"];
  }
  if (json.contains("data")) {
    json = json["data"];
  }
  if (json.contains("candles")) {
    json = json["candles"];
  }

  std::vector<HistoricalCandle> candles;
  if (json.is_array()) {
    candles.reserve(json.size());
    for (const auto& entry : json) {
      candles.push_back(parseHistoricalCandle(entry, token_mint, timeframe));
    }
  } else if (!json.is_null()) {
    candles.push_back(parseHistoricalCandle(json, token_mint, timeframe));
  }

  return candles;
}

PumpFunClient::SubscriptionId PumpFunClient::subscribeToQuotes(const std::string& token_mint,
                                                               QuoteCallback callback,
                                                               std::chrono::milliseconds interval) {
  if (!callback) {
    throw std::invalid_argument("Quote callback must be valid");
  }

  if (!running_.load()) {
    throw std::runtime_error("PumpFunClient is shutting down");
  }

  auto subscription = std::make_shared<Subscription>();
  subscription->token_mint = token_mint;
  subscription->callback = std::move(callback);
  subscription->interval = interval;

  const SubscriptionId id = next_subscription_id_.fetch_add(1);
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.emplace(id, subscription);
  }

  subscription->worker = std::thread([this, subscription]() {
    while (running_.load() && subscription->active.load()) {
      try {
        const TokenQuote quote = fetchTokenQuote(subscription->token_mint);
        subscription->callback(quote);
      } catch (const std::exception& ex) {
        std::cerr << "PumpFunClient quote polling error: " << ex.what() << std::endl;
      }

      const auto wake_time = std::chrono::steady_clock::now() + subscription->interval;
      while (running_.load() && subscription->active.load() && std::chrono::steady_clock::now() < wake_time) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
  });

  return id;
}

void PumpFunClient::unsubscribe(SubscriptionId id) {
  std::shared_ptr<Subscription> subscription;
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = subscriptions_.find(id);
    if (it == subscriptions_.end()) {
      return;
    }
    subscription = it->second;
    subscriptions_.erase(it);
  }

  if (subscription) {
    subscription->active.store(false);
    if (subscription->worker.joinable()) {
      subscription->worker.join();
    }
  }
}

void PumpFunClient::stopAll() {
  const bool was_running = running_.exchange(false);
  drainSubscriptions();
  running_.store(was_running);
}

TokenMetadata PumpFunClient::parseTokenMetadata(const nlohmann::json& json) const {
  TokenMetadata metadata;
  metadata.mint = json.value("mint", json.value("address", ""));
  metadata.name = json.value("name", "");
  metadata.symbol = json.value("symbol", "");
  metadata.description = json.value("description", json.value("desc", ""));
  metadata.image_url = json.value("image", json.value("imageUrl", json.value("image_url", "")));
  metadata.market_cap = json.value("marketCap", json.value("market_cap", 0.0));
  metadata.liquidity = json.value("liquidity", json.value("liquidityUsd", 0.0));
  metadata.holder_count = json.value("holderCount", json.value("holder_count", json.value("holders", 0ULL)));
  metadata.last_updated = json.value("updatedAt", json.value("updated_at", json.value("last_updated", "")));
  return metadata;
}

TokenQuote PumpFunClient::parseTokenQuote(const nlohmann::json& json) const {
  TokenQuote quote;
  quote.mint = json.value("mint", json.value("address", ""));
  quote.price = json.value("price", json.value("priceUsd", json.value("usdPrice", 0.0)));
  quote.price_change_24h = json.value("priceChange24h", json.value("price_change_24h", json.value("priceChange", 0.0)));
  quote.volume_24h = json.value("volume24h", json.value("volume_24h", json.value("volume", 0.0)));
  quote.liquidity = json.value("liquidity", json.value("liquidityUsd", 0.0));
  quote.timestamp = json.value("timestamp", json.value("updatedAt", json.value("time", "")));
  return quote;
}

HistoricalCandle PumpFunClient::parseHistoricalCandle(const nlohmann::json& json,
                                                      const std::string& token_mint,
                                                      const std::string& timeframe) const {
  HistoricalCandle candle;
  candle.mint = token_mint;
  candle.timeframe = timeframe;
  candle.open_time = json.value("open_time", json.value("startTime", json.value("time", "")));
  candle.close_time = json.value("close_time", json.value("closeTime", json.value("endTime", "")));
  candle.open = json.value("open", 0.0);
  candle.high = json.value("high", 0.0);
  candle.low = json.value("low", 0.0);
  candle.close = json.value("close", 0.0);
  candle.volume = json.value("volume", json.value("volumeUsd", 0.0));
  candle.quote_volume = json.value("quote_volume", json.value("quoteVolume", 0.0));
  return candle;
}

std::string PumpFunClient::buildUrl(
    const std::string& endpoint,
    const std::vector<std::pair<std::string, std::string>>& query_params) const {
  std::string url = base_url_;
  if (!endpoint.empty()) {
    if (!url.empty() && endpoint.front() != '/') {
      url += "/";
    }
    url += endpoint.front() == '/' ? endpoint.substr(1) : endpoint;
  }

  if (!query_params.empty()) {
    url.push_back('?');
    bool first = true;
    for (const auto& [key, value] : query_params) {
      if (!first) {
        url.push_back('&');
      }
      first = false;
      url += encodeQueryParam(key);
      url.push_back('=');
      url += encodeQueryParam(value);
    }
  }

  return url;
}

std::string PumpFunClient::performGet(
    const std::string& endpoint,
    const std::vector<std::pair<std::string, std::string>>& query_params,
    const std::unordered_map<std::string, std::string>& extra_headers) const {
  const std::string url = buildUrl(endpoint, query_params);

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL easy handle");
  }

  std::string buffer;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &PumpFunClient::curlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

  std::unordered_map<std::string, std::string> headers;
  {
    std::lock_guard<std::mutex> lock(http_mutex_);
    headers = default_headers_;
  }

  for (const auto& [key, value] : extra_headers) {
    headers[key] = value;
  }

  if (!api_key_.empty() && headers.find("x-api-key") == headers.end() && headers.find("X-API-Key") == headers.end()) {
    headers["x-api-key"] = api_key_;
  }

  struct curl_slist* header_list = nullptr;
  for (const auto& [key, value] : headers) {
    header_list = curl_slist_append(header_list, (key + ": " + value).c_str());
  }
  header_list = curl_slist_append(header_list, "Accept: application/json");

  if (header_list != nullptr) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  }

  const CURLcode result = curl_easy_perform(curl);
  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

  if (header_list != nullptr) {
    curl_slist_free_all(header_list);
  }
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    throw std::runtime_error(std::string("cURL request failed: ") + curl_easy_strerror(result));
  }

  if (status_code >= 400) {
    throw std::runtime_error("HTTP error " + std::to_string(status_code) + ": " + buffer);
  }

  return buffer;
}

std::string PumpFunClient::encodeQueryParam(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (unsigned char c : value) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      escaped << '%' << std::uppercase << std::setw(2)
              << static_cast<int>(static_cast<unsigned char>(c)) << std::nouppercase;
    }
  }

  return escaped.str();
}

size_t PumpFunClient::curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  const size_t total_size = size * nmemb;
  auto* buffer = static_cast<std::string*>(userp);
  buffer->append(static_cast<char*>(contents), total_size);
  return total_size;
}

void PumpFunClient::drainSubscriptions() {
  std::unordered_map<SubscriptionId, std::shared_ptr<Subscription>> local;
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    local.swap(subscriptions_);
  }

  for (auto& [id, subscription] : local) {
    (void)id;
    if (!subscription) {
      continue;
    }
    subscription->active.store(false);
    if (subscription->worker.joinable()) {
      subscription->worker.join();
    }
  }
}

}  // namespace market_data

