#include "totp.h"

#include <openssl/hmac.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace security {
namespace {
constexpr std::uint64_t kTimeStepSeconds = 30;
constexpr std::size_t kMinTotpDigits = 6;
constexpr std::size_t kMaxTotpDigits = 8;

int char_to_base32(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= '2' && c <= '7') {
        return 26 + (c - '2');
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }
    return -1;
}

std::string zero_pad(std::uint32_t value, std::size_t digits) {
    std::ostringstream stream;
    stream << std::setw(static_cast<int>(digits)) << std::setfill('0') << value;
    return stream.str();
}

}  // namespace

TotpValidator::TotpValidator() = default;

bool TotpValidator::validate(const std::string &base32_secret, const std::string &code, int allowed_drift,
                             std::chrono::system_clock::time_point now) const {
    const auto isDigit = [](unsigned char c) { return std::isdigit(c) != 0; };
    if (code.size() < kMinTotpDigits || code.size() > kMaxTotpDigits ||
        !std::all_of(code.begin(), code.end(), [&](char c) { return isDigit(static_cast<unsigned char>(c)); })) {
        return false;
    }

    const std::size_t digits = code.size();
    const auto secret = base32_decode(base32_secret);
    if (secret.empty()) {
        return false;
    }

    const auto epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto counter = static_cast<std::int64_t>(epoch_seconds / kTimeStepSeconds);

    for (int offset = -allowed_drift; offset <= allowed_drift; ++offset) {
        const auto candidate = counter + offset;
        if (candidate < 0) {
            continue;
        }
        const std::uint32_t generated =
            generate_totp(secret, static_cast<std::uint64_t>(candidate), digits);
        if (zero_pad(generated, digits) == code) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint8_t> TotpValidator::base32_decode(const std::string &input) {
    std::vector<std::uint8_t> output;
    output.reserve((input.size() * 5) / 8);

    int buffer = 0;
    int bits_left = 0;
    for (char c : input) {
        if (c == '=') {
            break;
        }
        const int value = char_to_base32(c);
        if (value < 0) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                throw std::runtime_error("Invalid character in base32 secret");
            }
            continue;
        }
        buffer = (buffer << 5) | value;
        bits_left += 5;
        if (bits_left >= 8) {
            bits_left -= 8;
            output.push_back(static_cast<std::uint8_t>((buffer >> bits_left) & 0xFF));
        }
    }

    return output;
}

std::uint32_t TotpValidator::generate_totp(const std::vector<std::uint8_t> &secret,
                                           std::uint64_t counter,
                                           std::size_t digits) {
    std::array<unsigned char, sizeof(counter)> counter_bytes{};
    auto moving_counter = counter;
    for (int i = static_cast<int>(counter_bytes.size()) - 1; i >= 0; --i) {
        counter_bytes[static_cast<std::size_t>(i)] = static_cast<unsigned char>(moving_counter & 0xFF);
        moving_counter >>= 8;
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (HMAC(EVP_sha1(), secret.data(), static_cast<int>(secret.size()), counter_bytes.data(),
             counter_bytes.size(), hash, &hash_len) == nullptr) {
        throw std::runtime_error("Unable to compute HMAC-SHA1 for TOTP");
    }

    const int offset = hash[hash_len - 1] & 0x0F;
    const std::uint32_t binary = ((hash[offset] & 0x7F) << 24) | ((hash[offset + 1] & 0xFF) << 16) |
                                 ((hash[offset + 2] & 0xFF) << 8) | (hash[offset + 3] & 0xFF);

    std::uint32_t mod = 1;
    for (std::size_t i = 0; i < digits; ++i) {
        mod *= 10;
    }
    return binary % mod;
}

}  // namespace security
