# Operations Runbook

This runbook documents the day-2 procedures for the current memecoinbot MVP.
The stack is composed of the risk-managed engine, Pump.fun HTTP client, security
primitives (secret store + TOTP), the Telegram bot, and the ImGui console.

## Environments

* **Build hosts** – Linux x86_64, GCC ≥13, CMake ≥3.16
* **Runtime** – systemd service or tmux session is sufficient (no container
  image provided)
* **Secrets** – encrypted via `security::SecretStore` and stored outside the
  repo

## Bootstrapping a node

1. Install dependencies: `build-essential`, `cmake`, `libcurl4-openssl-dev`,
   `libssl-dev`, optional `tgbot-cpp`.
2. Clone the repository and populate `.env` or export variables as per
   `.env.example`.
3. Compile the code:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build build
   ```
4. Run the regression tests before enabling trading:
   ```bash
   ctest --test-dir build
   ```

## Starting components

* **Trading engine (headless demo)**
  ```bash
  ./build/trading_engine_app
  ```
  The program seeds the engine with two sample orders and exits. Replace this
  with your integration harness for production flows.

* **ImGui console** – requires a platform renderer backend. The stub demonstrates
  layout logic only. Embed `ui::TradingImGuiApp` into your render loop and call
  `attachEngine` with your `TradingEngine` instance.

* **Telegram bot** – link `telegram::TelegramBot` against a real `TgBot::Bot`
  token and run `start()`. Ensure the encrypted secret store already contains
  `telegram/totp/<chat_id>` entries before enabling trade commands.

## Stopping components

All long-running helpers (`RiskManagedEngine`, `TelegramBot`, Pump.fun polling)
expose `stop()`/`stopAll()` to terminate threads. Always call these before
process exit to avoid orphaned workers.

## Key & secret rotation

1. Unlock the secret store with the master password.
2. Call `SecretStore::set_secret` for updated values (API keys, wallet seeds,
   Telegram TOTP secrets).
3. Persist with `SecretStore::save` and distribute the encrypted blob to nodes.
4. Remove stale keys with `SecretStore::erase_secret` and re-save.

## Pump.fun rate limiting

* Requests are synchronous (`libcurl`) with a 10 second timeout.
* There is **no** retry/backoff; wrap `PumpFunClient::fetch*` with your own
  retry policy if required.
* Quote polling spawns one thread per subscription. Use `stopAll()` before
  shutdown to join threads.

## Monitoring & telemetry

* Logging currently uses stdout/stderr. Pipe into journald or a log collector.
* Track repeated TOTP failures and Pump.fun HTTP errors; both are surfaced via
  `std::cerr` in the existing code.

## Incident response checklist

1. Stop the trading engine and Telegram bot via their `stop()` methods.
2. Rotate any compromised credentials in the secret store.
3. Re-run the full test suite and ASAN build before restarting services.
4. Document the incident in the release checklist audit trail.
