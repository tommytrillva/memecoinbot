#include "trading/engine.h"

#include <chrono>
#include <thread>

int main() {
    trading::RiskManagedEngine engine({50.0, 200.0});
    engine.start();

    trading::OrderRequest btc_buy{"BTC-USD", 10.0, 30000.0};
    engine.buy(btc_buy);

    trading::OrderRequest eth_sell{"ETH-USD", 5.0, 2000.0};
    engine.sell(eth_sell);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    engine.stop();
    return 0;
}
