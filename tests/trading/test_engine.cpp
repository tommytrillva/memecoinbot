#include "trading/engine.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
    }
    return condition;
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

}  // namespace

int main() {
    if (!TestRejectsWhenStopped()) {
        return 1;
    }
    if (!TestRiskLimits()) {
        return 1;
    }
    return 0;
}
