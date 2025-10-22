#include "market_data/pumpfun_client.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace market_data {

class PumpFunClientTestPeer {
 public:
  static std::string BuildUrl(const PumpFunClient& client,
                              const std::string& endpoint,
                              const std::vector<std::pair<std::string, std::string>>& query_params = {}) {
    return client.buildUrl(endpoint, query_params);
  }
};

}  // namespace market_data

bool TestUrlBuilder() {
  market_data::PumpFunClient client("https://api.example.com/v1");
  const std::string built_url =
      market_data::PumpFunClientTestPeer::BuildUrl(client, "/metadata");
  if (built_url != "https://api.example.com/v1/metadata") {
    std::cerr << "Expected https://api.example.com/v1/metadata but got " << built_url << std::endl;
    return false;
  }
  return true;
}

bool TestRetriesSucceed() {
  std::atomic<int> attempts{0};
  market_data::PumpFunClient client(
      "https://api.example.com",
      {},
      "/metadata",
      "/quotes",
      "/candles",
      [&attempts](const std::string&, const std::vector<std::pair<std::string, std::string>>&,
                  const std::unordered_map<std::string, std::string>&) -> std::string {
        const int attempt = ++attempts;
        if (attempt < 3) {
          throw std::runtime_error("transient error");
        }
        return R"({"mint":"TOKEN","price":1.23,"liquidity":1000})";
      });

  client.setRetryPolicy(3, std::chrono::milliseconds(0));

  try {
    const auto quote = client.fetchTokenQuote("TOKEN");
    if (attempts.load() != 3) {
      std::cerr << "Expected 3 attempts but saw " << attempts.load() << std::endl;
      return false;
    }
    if (quote.mint != "TOKEN") {
      std::cerr << "Unexpected mint: " << quote.mint << std::endl;
      return false;
    }
    if (std::abs(quote.price - 1.23) > 1e-6) {
      std::cerr << "Unexpected price: " << quote.price << std::endl;
      return false;
    }
  } catch (const std::exception& ex) {
    std::cerr << "fetchTokenQuote threw unexpectedly: " << ex.what() << std::endl;
    return false;
  }

  return true;
}

bool TestRetriesExhaust() {
  std::atomic<int> attempts{0};
  market_data::PumpFunClient client(
      "https://api.example.com",
      {},
      "/metadata",
      "/quotes",
      "/candles",
      [&attempts](const std::string&, const std::vector<std::pair<std::string, std::string>>&,
                  const std::unordered_map<std::string, std::string>&) -> std::string {
        ++attempts;
        throw std::runtime_error("persistent error");
      });

  client.setRetryPolicy(2, std::chrono::milliseconds(0));

  try {
    client.fetchTokenQuote("TOKEN");
    std::cerr << "Expected fetchTokenQuote to throw after retries" << std::endl;
    return false;
  } catch (const std::exception&) {
    if (attempts.load() != 2) {
      std::cerr << "Expected 2 attempts but saw " << attempts.load() << std::endl;
      return false;
    }
  }

  return true;
}

bool TestMetadataParsing() {
  market_data::PumpFunClient client(
      "https://api.example.com",
      {},
      "/metadata",
      "/quotes",
      "/candles",
      [](const std::string&, const std::vector<std::pair<std::string, std::string>>&,
         const std::unordered_map<std::string, std::string>&) -> std::string {
        return R"({
          "result": {
            "data": {
              "metadata": {
                "mint": "TOKEN",
                "name": "My Token",
                "symbol": "MTKN",
                "marketCap": 42.5,
                "liquidity": 12.5,
                "holderCount": 7,
                "updatedAt": "2024-01-01T00:00:00Z"
              }
            }
          }
        })";
      });

  try {
    const auto metadata = client.fetchTokenMetadata("TOKEN");
    if (metadata.mint != "TOKEN" || metadata.symbol != "MTKN" || metadata.market_cap != 42.5) {
      std::cerr << "Metadata fields did not parse as expected: mint='" << metadata.mint
                << "' symbol='" << metadata.symbol << "' market_cap=" << metadata.market_cap << std::endl;
      return false;
    }
  } catch (const std::exception& ex) {
    std::cerr << "fetchTokenMetadata threw unexpectedly: " << ex.what() << std::endl;
    return false;
  }

  return true;
}

int main() {
  if (!TestUrlBuilder()) {
    return 1;
  }
  if (!TestRetriesSucceed()) {
    return 1;
  }
  if (!TestRetriesExhaust()) {
    return 1;
  }
  if (!TestMetadataParsing()) {
    return 1;
  }
  return 0;
}
