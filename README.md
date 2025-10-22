# memecoinbot

Memecoinbot is a C++ reference stack for experimenting with a memecoin trading
bot. The repository focuses on the MVP surface that already exists in the code
base:

* **Trading engine** – in-process risk controls and asynchronous order routing
  built around `trading::RiskManagedEngine`.
* **Pump.fun market-data client** – HTTP polling utilities for fetching token
  metadata, quotes, and candles through QuickNode/Moralis style endpoints.
* **Security primitives** – AES-256-GCM encrypted secret store, RFC 6238
  TOTP validator, and an Ed25519 Solana signer for wallet operations.
* **Telegram control plane** – optional command bot with mandatory TOTP codes
  forwarded through the validation client before trades reach the engine.
* **Dear ImGui console** – lightweight UI scaffold for manual interaction.

> ⚠️ Phantom wallet signing, real trade submission, and end-to-end venue
> integration are **not** implemented. The current code only simulates routing
> and risk management.

## Prerequisites

* CMake ≥ 3.16
* A C++17 toolchain (tested with GCC 13.3)
* libcurl, OpenSSL, pthreads development headers
* Optional: [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp) and Dear ImGui
  backend glue if you intend to integrate the Telegram bot or UI demo

On Debian/Ubuntu you can install the dependencies with:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libssl-dev
```

## Configure & build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

### Tests

```bash
ctest --test-dir build
```

The test suite includes:

* HTTP helper coverage (`pumpfun_client_tests`)
* Secret store + TOTP validation round-trips (`security_tests`)
* Trading engine risk-limit behaviour (`trading_engine_tests`)

### Sanitizers

Address/Undefined sanitizers can be enabled via:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan
```

## Running the demos

### Trading engine sample

```bash
./build/trading_engine_app
```

The binary starts the risk-managed engine, submits two example orders, waits for
processing, and exits. Execution is logged to stdout.

### ImGui console stub

```bash
./build/trading_ui_demo
```

This demo wires the engine into `ui::TradingImGuiApp` and simulates three
frames. Integrators must hook in a real renderer/windowing backend to display
widgets.

## Configuration

Populate a `.env` (see `.env.example`) or load values into your process
environment before launching custom integrations. Core variables:

* `PUMPFUN_BASE_URL` / `PUMPFUN_API_KEY`
* `SOLANA_RPC_URL`
* `PHANTOM_WALLET_KEYPAIR`
* `TELEGRAM_BOT_TOKEN` / `TELEGRAM_ALLOWED_CHAT_IDS`
* `SECRET_STORE_PATH` / `SECRET_STORE_PASSWORD`

The encrypted secret store helper lives in `src/security/secret_store.cpp`. Use
it to persist API keys, wallet material, and Telegram TOTP seeds.

## Repository layout

```
src/
  trading/        Risk-managed engine core
  market_data/    Pump.fun REST client
  security/       Secret store + TOTP validation
  telegram/       Bot command surface and TOTP client helper
  ui/             Dear ImGui façade

tests/
  market_data/    PumpFunClient helper coverage
  security/       Secret store + TOTP regression tests
  trading/        Engine behavioural tests
```

## Known gaps

* Venue/exchange adapters remain stubs; orders are still simulated
* Pump.fun integration uses polling subscriptions—no websocket streaming yet
* Dear ImGui console still lacks a production renderer/backend

These items are tracked in the accompanying assessment report and gap plan.
