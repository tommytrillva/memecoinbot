# Release Checklist

Use this checklist to decide whether the current commit is shippable. Attach the
completed list to the release ticket.

## Pre-flight

- [ ] Update CHANGELOG / release notes (if applicable)
- [ ] Confirm `.env` / secret store paths for target environment
- [ ] Ensure Phantom wallet integration status is documented (still pending)

## Build & test

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`
- [ ] `cmake --build build`
- [ ] `ctest --test-dir build`
- [ ] `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \`
      `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`
- [ ] `cmake --build build-asan`
- [ ] `ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan`

## Security

- [ ] Rotate/review Pump.fun API keys and Telegram TOTP secrets
- [ ] Run `gitleaks` (or equivalent) to ensure secrets are not committed
- [ ] Validate secret store contents using `tests/security/test_security.cpp`

## Operational readiness

- [ ] Verify logging sinks (stdout/stderr piping) in staging
- [ ] Smoke-test Telegram bot commands in a staging chat (with TOTP validation)
- [ ] Document any open gaps or incident learnings in `docs/runbook.md`

## Sign-off

- [ ] Engineering lead approval
- [ ] Security approval (if secrets or auth paths changed)
