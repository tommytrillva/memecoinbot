#include "security/secret_store.h"
#include "security/solana_signer.h"
#include "security/totp.h"

#include <openssl/evp.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool TestTotpValidation() {
    security::TotpValidator validator;
    const std::string secret = "JBSWY3DPEHPK3PXP";  // Base32 for RFC 6238 style fixture.
    const auto fixed_time = std::chrono::system_clock::time_point{std::chrono::seconds{1234567890}};

    if (!validator.validate(secret, "742275", 1, fixed_time)) {
        std::cerr << "TOTP validator failed to accept known-good code" << std::endl;
        return false;
    }

    if (!validator.validate(secret, "94742275", 1, fixed_time)) {
        std::cerr << "TOTP validator failed to accept known-good 8-digit code" << std::endl;
        return false;
    }

    if (validator.validate(secret, "000000", 1, fixed_time)) {
        std::cerr << "TOTP validator accepted an invalid code" << std::endl;
        return false;
    }

    if (validator.validate(secret, "74227", 1, fixed_time)) {
        std::cerr << "TOTP validator accepted a short code" << std::endl;
        return false;
    }

    if (validator.validate(secret, "947422750", 1, fixed_time)) {
        std::cerr << "TOTP validator accepted an overlong code" << std::endl;
        return false;
    }

    return true;
}

bool TestSecretStoreRoundTrip() {
    const std::string password = "unit-test-master";
    security::SecretStore store(password);
    store.set_secret("pumpfun/api_key", "test-key-123");
    store.set_secret("telegram/totp/123", "JBSWY3DPEHPK3PXP");

    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("memecoinbot_secret_store_" + std::to_string(timestamp) + ".bin");

    try {
        store.save(path);
    } catch (const std::exception& ex) {
        std::cerr << "SecretStore save failed: " << ex.what() << std::endl;
        return false;
    }

    bool success = true;
    try {
        security::SecretStore reloaded(password);
        reloaded.load(path);
        const auto api_key = reloaded.get_secret("pumpfun/api_key");
        const auto totp_secret = reloaded.get_secret("telegram/totp/123");
        if (!api_key || *api_key != "test-key-123") {
            std::cerr << "SecretStore round-trip lost API key" << std::endl;
            success = false;
        }
        if (!totp_secret || *totp_secret != "JBSWY3DPEHPK3PXP") {
            std::cerr << "SecretStore round-trip lost TOTP secret" << std::endl;
            success = false;
        }
        const auto keys = reloaded.list_keys();
        if (keys.size() != 2) {
            std::cerr << "SecretStore expected 2 keys but saw " << keys.size() << std::endl;
            success = false;
        }
    } catch (const std::exception& ex) {
        std::cerr << "SecretStore load failed: " << ex.what() << std::endl;
        success = false;
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);

    return success;
}

bool TestSolanaSigner() {
    const std::string keypair =
        "49W385L4rePHy6PAaQUovbD2aacgN4HsKXSMeUzRg4fmwXszN91JuMFrQRj3vMDpZuRF3ZknQBuRBoWQJEfXstMw";
    const std::string expected_public = "FVen3X669xLzsi6N2V91DoiyzHzg1uAgqiT8jZ9nS96Z";
    const std::vector<std::uint8_t> message{'h', 'e', 'l', 'l', 'o'};

    try {
        security::SolanaSigner signer = security::SolanaSigner::FromBase58(keypair);

        if (signer.publicKeyBase58() != expected_public) {
            std::cerr << "Unexpected public key base58 representation" << std::endl;
            return false;
        }

        const auto signature = signer.signMessage(message);

        EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
            EVP_PKEY_ED25519, nullptr, signer.publicKey().data(), signer.publicKey().size());
        if (!key) {
            std::cerr << "Failed to create public key for verification" << std::endl;
            return false;
        }

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            EVP_PKEY_free(key);
            std::cerr << "Failed to allocate verification context" << std::endl;
            return false;
        }

        bool success = true;
        if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) != 1) {
            std::cerr << "EVP_DigestVerifyInit failed" << std::endl;
            success = false;
        } else if (EVP_DigestVerify(ctx, signature.data(), signature.size(), message.data(), message.size()) != 1) {
            std::cerr << "Signature verification failed" << std::endl;
            success = false;
        }

        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);
        return success;
    } catch (const std::exception& ex) {
        std::cerr << "Solana signer test error: " << ex.what() << std::endl;
        return false;
    }
}

}  // namespace

int main() {
    if (!TestTotpValidation()) {
        return 1;
    }
    if (!TestSecretStoreRoundTrip()) {
        return 1;
    }
    if (!TestSolanaSigner()) {
        return 1;
    }
    return 0;
}
