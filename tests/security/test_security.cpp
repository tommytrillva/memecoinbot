#include "security/secret_store.h"
#include "security/totp.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool TestTotpValidation() {
    security::TotpValidator validator;
    const std::string secret = "JBSWY3DPEHPK3PXP";  // Base32 for RFC 6238 style fixture.
    const auto fixed_time = std::chrono::system_clock::time_point{std::chrono::seconds{1234567890}};

    if (!validator.validate(secret, "742275", 1, fixed_time)) {
        std::cerr << "TOTP validator failed to accept known-good code" << std::endl;
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

}  // namespace

int main() {
    if (!TestTotpValidation()) {
        return 1;
    }
    if (!TestSecretStoreRoundTrip()) {
        return 1;
    }
    return 0;
}
