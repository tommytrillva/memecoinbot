#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace security {

// TotpValidator implements RFC 6238 compatible validation of time-based
// one-time passwords (TOTP). Secrets are provided as base32-encoded strings
// as exported by the majority of authenticator applications.
class TotpValidator {
  public:
    TotpValidator();

    // Validates the provided |code| against the secret. |code| should contain
    // between six and eight digits. |allowed_drift| indicates the number of 30 second
    // windows to check on either side of the current time to account for
    // small clock differences.
    bool validate(const std::string &base32_secret, const std::string &code,
                  int allowed_drift = 1,
                  std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

  private:
    static std::vector<std::uint8_t> base32_decode(const std::string &input);
    static std::uint32_t generate_totp(const std::vector<std::uint8_t> &secret,
                                       std::uint64_t counter,
                                       std::size_t digits);
};

}  // namespace security
