#include "trading/engine.h"
#include "trading/pumpfun_bridge.h"

#include "market_data/pumpfun_client.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
    }
    return condition;
}

bool WaitForCondition(std::function<bool()> predicate,
                      std::chrono::milliseconds timeout,
                      std::chrono::milliseconds poll_interval = std::chrono::milliseconds(10)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(poll_interval);
    }
    return predicate();
}

bool TestRejectsWhenStopped() {
    trading::RiskManagedEngine engine;
    trading::OrderRequest request;
    request.symbol = "TEST";
    request.quantity = 1.0;
    const auto receipt = engine.buy(request);
    return Expect(!receipt.success, "Engine accepted order while stopped");
}

bool TestRiskLimits() {
    trading::RiskManagedEngine engine;
    trading::RiskLimits limits;
    limits.maxPosition = 5.0;
    limits.maxExposure = 10.0;
    engine.updateRiskLimits(limits);
    engine.start();

    engine.updateMarkPrice("COIN", 1.0);

    trading::OrderRequest first;
    first.symbol = "COIN";
    first.quantity = 4.0;
    auto first_receipt = engine.buy(first);
    if (!Expect(first_receipt.success, "Engine failed to queue initial order")) {
        engine.stop();
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    trading::OrderRequest second;
    second.symbol = "COIN";
    second.quantity = 3.0;
    auto second_receipt = engine.buy(second);
    engine.stop();
    return Expect(!second_receipt.success, "Engine failed to enforce position limit");
}

bool TestExposureLimitUsesMarkPrice() {
    trading::RiskManagedEngine engine;
    trading::RiskLimits limits;
    limits.maxPosition = 100.0;
    limits.maxExposure = 50.0;
    engine.updateRiskLimits(limits);
    engine.start();

    engine.updateMarkPrice("COIN", 25.0);

    trading::OrderRequest request;
    request.symbol = "COIN";
    request.quantity = 3.0;  // Notional 75, exceeds exposure limit

    auto receipt = engine.buy(request);
    engine.stop();

    return Expect(!receipt.success,
                  "Engine accepted order despite mark-price derived exposure breach");
}

bool TestPumpFunBridgePropagatesMarkPrice() {
    std::atomic<int> fetch_count{0};

    market_data::PumpFunClient client(
        "https://api.example.com",
        {},
        "/metadata",
        "/quotes",
        "/candles",
        [&fetch_count](const std::string&, const std::vector<std::pair<std::string, std::string>>&,
                       const std::unordered_map<std::string, std::string>&) -> std::string {
            fetch_count.fetch_add(1);
            return R"({"mint":"TOKEN","price":25.0})";
        });
    client.setRetryPolicy(1, std::chrono::milliseconds(0));

    trading::RiskManagedEngine engine;
    trading::RiskLimits limits;
    limits.maxPosition = 100.0;
    limits.maxExposure = 50.0;
    engine.updateRiskLimits(limits);
    engine.start();

    trading::PumpFunMarketDataBridge bridge(client, engine);
    bridge.start({"TOKEN"}, std::chrono::milliseconds(10));

    if (!WaitForCondition([&fetch_count]() { return fetch_count.load() > 0; },
                          std::chrono::milliseconds(500))) {
        bridge.stop();
        client.stopAll();
        engine.stop();
        return Expect(false, "PumpFun bridge did not fetch any quotes");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    trading::OrderRequest request;
    request.symbol = "TOKEN";
    request.quantity = 3.0;  // Notional 75, exceeds exposure limit once price propagates

    auto receipt = engine.buy(request);

    bridge.stop();
    client.stopAll();
    engine.stop();

    return Expect(!receipt.success,
                  "Engine accepted order despite Pump.fun mark price exposure breach");
}

}  // namespace

int main() {
    if (!TestRejectsWhenStopped()) {
        return 1;
    }
    if (!TestRiskLimits()) {
        return 1;
    }
    if (!TestExposureLimitUsesMarkPrice()) {
        return 1;
    }
    if (!TestPumpFunBridgePropagatesMarkPrice()) {
        return 1;
    }
    return 0;
}
