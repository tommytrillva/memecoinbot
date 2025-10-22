#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace security {

class SolanaSigner {
public:
    static SolanaSigner FromBase58(const std::string& keypair);
    static SolanaSigner FromBytes(const std::vector<std::uint8_t>& keypair_bytes);

    std::vector<std::uint8_t> signMessage(const std::vector<std::uint8_t>& message) const;

    std::array<std::uint8_t, 32> publicKey() const;
    std::string publicKeyBase58() const;

private:
    SolanaSigner(std::array<std::uint8_t, 32> secret_key,
                 std::array<std::uint8_t, 32> public_key);

    static std::vector<std::uint8_t> decodeBase58(const std::string& input);
    static std::string encodeBase58(const std::uint8_t* data, std::size_t length);
    static std::array<std::uint8_t, 32> derivePublicKey(const std::array<std::uint8_t, 32>& secret_key);

    std::array<std::uint8_t, 32> secret_key_{};
    std::array<std::uint8_t, 32> public_key_{};
};

}  // namespace security
