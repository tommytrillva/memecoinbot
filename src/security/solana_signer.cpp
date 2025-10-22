#include "security/solana_signer.h"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace security {
namespace {
constexpr char kBase58Alphabet[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::array<int, 256> buildBase58Indexes() {
    std::array<int, 256> indexes{};
    indexes.fill(-1);
    for (int i = 0; kBase58Alphabet[i] != '\0'; ++i) {
        indexes[static_cast<unsigned char>(kBase58Alphabet[i])] = i;
    }
    return indexes;
}

const std::array<int, 256>& base58Indexes() {
    static const auto indexes = buildBase58Indexes();
    return indexes;
}

std::array<std::uint8_t, 32> arrayFromVector(const std::vector<std::uint8_t>& input,
                                             std::size_t offset = 0) {
    if (input.size() < offset + 32) {
        throw std::invalid_argument("Input does not contain enough bytes");
    }
    std::array<std::uint8_t, 32> array{};
    std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(offset), 32, array.begin());
    return array;
}

}  // namespace

SolanaSigner::SolanaSigner(std::array<std::uint8_t, 32> secret_key,
                           std::array<std::uint8_t, 32> public_key)
    : secret_key_(secret_key), public_key_(public_key) {}

SolanaSigner SolanaSigner::FromBase58(const std::string& keypair) {
    return FromBytes(decodeBase58(keypair));
}

SolanaSigner SolanaSigner::FromBytes(const std::vector<std::uint8_t>& keypair_bytes) {
    if (keypair_bytes.size() != 32 && keypair_bytes.size() != 64) {
        throw std::invalid_argument("Solana keypair must be 32 or 64 bytes long");
    }

    std::array<std::uint8_t, 32> secret = arrayFromVector(keypair_bytes, 0);
    std::array<std::uint8_t, 32> derived_public = derivePublicKey(secret);

    if (keypair_bytes.size() == 64) {
        std::array<std::uint8_t, 32> provided_public = arrayFromVector(keypair_bytes, 32);
        if (!std::equal(provided_public.begin(), provided_public.end(),
                        derived_public.begin())) {
            throw std::invalid_argument("Provided public key does not match private key");
        }
    }

    return SolanaSigner(secret, derived_public);
}

std::vector<std::uint8_t> SolanaSigner::signMessage(const std::vector<std::uint8_t>& message) const {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr, secret_key_.data(), secret_key_.size());
    if (!key) {
        throw std::runtime_error("Failed to construct Ed25519 key");
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(key);
        throw std::runtime_error("Failed to allocate digest context");
    }

    std::vector<std::uint8_t> signature;
    size_t signature_size = 0;
    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);
        throw std::runtime_error("EVP_DigestSignInit failed");
    }

    if (EVP_DigestSign(ctx, nullptr, &signature_size, message.data(), message.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);
        throw std::runtime_error("Failed to determine signature length");
    }

    signature.resize(signature_size);
    if (EVP_DigestSign(ctx, signature.data(), &signature_size, message.data(), message.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(key);
        throw std::runtime_error("EVP_DigestSign failed");
    }
    signature.resize(signature_size);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return signature;
}

std::array<std::uint8_t, 32> SolanaSigner::publicKey() const {
    return public_key_;
}

std::string SolanaSigner::publicKeyBase58() const {
    return encodeBase58(public_key_.data(), public_key_.size());
}

std::vector<std::uint8_t> SolanaSigner::decodeBase58(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    std::vector<std::uint8_t> bytes;
    for (char ch : input) {
        const unsigned char c = static_cast<unsigned char>(ch);
        const int value = base58Indexes()[c];
        if (value < 0) {
            throw std::invalid_argument("Base58 string contains invalid characters");
        }

        int carry = value;
        for (auto& byte : bytes) {
            int x = byte * 58 + carry;
            byte = static_cast<std::uint8_t>(x & 0xFF);
            carry = x >> 8;
        }
        while (carry > 0) {
            bytes.push_back(static_cast<std::uint8_t>(carry & 0xFF));
            carry >>= 8;
        }
    }

    std::size_t leading_zeros = 0;
    while (leading_zeros < input.size() && input[leading_zeros] == '1') {
        ++leading_zeros;
    }

    for (std::size_t i = 0; i < leading_zeros; ++i) {
        bytes.push_back(0);
    }

    std::vector<std::uint8_t> result(bytes.rbegin(), bytes.rend());

    return result;
}

std::string SolanaSigner::encodeBase58(const std::uint8_t* data, std::size_t length) {
    if (length == 0) {
        return {};
    }

    std::size_t leading_zeros = 0;
    while (leading_zeros < length && data[leading_zeros] == 0) {
        ++leading_zeros;
    }

    std::vector<std::uint8_t> digits(1, 0);
    for (std::size_t i = leading_zeros; i < length; ++i) {
        int carry = data[i];
        for (auto& digit : digits) {
            int x = digit * 256 + carry;
            digit = static_cast<std::uint8_t>(x % 58);
            carry = x / 58;
        }
        while (carry > 0) {
            digits.push_back(static_cast<std::uint8_t>(carry % 58));
            carry /= 58;
        }
    }

    std::size_t non_zero = digits.size();
    while (non_zero > 0 && digits[non_zero - 1] == 0) {
        --non_zero;
    }

    std::string result(leading_zeros, '1');
    for (std::size_t i = non_zero; i > 0; --i) {
        result.push_back(kBase58Alphabet[digits[i - 1]]);
    }
    return result;
}

std::array<std::uint8_t, 32> SolanaSigner::derivePublicKey(
    const std::array<std::uint8_t, 32>& secret_key) {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr, secret_key.data(), secret_key.size());
    if (!key) {
        throw std::runtime_error("Failed to derive Ed25519 key");
    }

    std::array<std::uint8_t, 32> public_key{};
    size_t public_key_size = public_key.size();
    if (EVP_PKEY_get_raw_public_key(key, public_key.data(), &public_key_size) != 1) {
        EVP_PKEY_free(key);
        throw std::runtime_error("Failed to derive public key");
    }
    if (public_key_size != public_key.size()) {
        EVP_PKEY_free(key);
        throw std::runtime_error("Unexpected public key length");
    }

    EVP_PKEY_free(key);
    return public_key;
}

}  // namespace security
