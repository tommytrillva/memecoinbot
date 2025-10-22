#include "market_data/pumpfun_client.h"

#include <iostream>
#include <string>
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

int main() {
  market_data::PumpFunClient client("https://api.example.com/v1");
  const std::string built_url =
      market_data::PumpFunClientTestPeer::BuildUrl(client, "/metadata");

  if (built_url != "https://api.example.com/v1/metadata") {
    std::cerr << "Expected https://api.example.com/v1/metadata but got " << built_url << std::endl;
    return 1;
  }

  return 0;
}
