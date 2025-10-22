# Operational Security

This document explains how secrets are managed, the precautions that surround
Telegram two-factor authentication (2FA), and the checks we run to keep the
Memecoin trading stack hardened.

## Encrypted configuration

The bot stores API credentials, webhooks, and TOTP seeds in the encrypted
secret store implemented in `src/security/secret_store.cpp`. Secrets are stored
in-memory only after decrypting with the operator provided master password.
The file format uses AES-256-GCM with PBKDF2 (150k iterations) for key
stretching and a random salt/IV per encryption.

Usage guidelines:

1. Provision a strong master password that is **never** committed to source
   control.
2. Create or rotate secrets with a secure workstation. The helper binary should
   call `SecretStore::set_secret` and `SecretStore::save` once the new values
   have been provided.
3. Keep the encrypted blob under strict ACLs. The file is safe to back up, but
   prefer offline storage or a password manager vault where possible.
4. Inspect the `list_keys()` output during audits to verify that no stale API
   keys linger after rotation.

## Telegram 2FA requirements

Trades issued via Telegram must include a valid 6-digit TOTP code. Every chat
has a unique secret stored under the `telegram/totp/<chat_id>` key inside the
secret store. The `TelegramClient` fetches the secret, validates the code across
clock-drift windows, and only passes authorised requests to the execution layer.

Operational checklist:

- Reset TOTP seeds whenever operators change devices.
- Confirm system time synchronisation (`systemd-timesyncd`, `chrony`, or NTP)
  on the host running the bot. Large skews will cause false negatives.
- Log failed 2FA attempts and alert on repeated failures from a single chat.

## Solana signing

`security::SolanaSigner` converts Base58-encoded Solana keypairs into raw
Ed25519 key material. Use it to sign transaction payloads after retrieving the
encrypted keypair from the secret store. Always verify signatures with the
derived public key before broadcasting and rotate keys through the standard
secret rotation workflow.

## Dependency and binary auditing

Regular dependency and binary integrity audits are required to keep third-party
risk in check.

1. **Source dependencies** – run `npm audit`, `cargo audit`, or the equivalent
   tool for each language runtime used by the workspace. Resolve high severity
   issues immediately; medium severity issues should be triaged within a sprint.
2. **System packages** – monitor operating system updates and patch CVEs within
   48 hours. Use unattended upgrades or a configuration management tool to
   enforce the policy.
3. **Binary provenance** – recompile internal binaries from source during every
   release cycle. Verify the SHA-256 checksum of distributables before pushing
   them to production nodes.
4. **Secrets scanning** – execute `gitleaks` before each release to guarantee no
   sensitive material is accidentally committed.

Document the run of each audit (date, tool version, summary of findings) in the
runbook so the data is available during post-incident reviews.
