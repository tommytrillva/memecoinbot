#include "trading/engine.h"

#include <chrono>
#include <thread>

int main() {
    trading::TradingEngine engine({50.0, 200.0});
    engine.start();

    engine.submitOrder({"BTC-USD", 10.0, 30000.0});
    engine.submitOrder({"ETH-USD", 5.0, 2000.0});

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    engine.stop();
    return 0;
}
