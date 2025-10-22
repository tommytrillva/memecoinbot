#include "trading/engine.h"
#include "ui/imgui_trading_app.h"

#include <chrono>
#include <memory>
#include <thread>

int main() {
    auto engine = std::make_shared<trading::TradingEngine>(trading::RiskLimits{50.0, 200.0});
    ui::TradingImGuiApp app;
    app.attachEngine(engine);
    app.initialize();

    engine->start();

    // Simulate a small number of frames for demonstration purposes. Integrators
    // should replace this loop with their platform-specific event/render loop.
    for (int frame = 0; frame < 3; ++frame) {
        app.beginFrame();
        app.render();
        app.endFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    engine->stop();
    app.shutdown();
    return 0;
}

