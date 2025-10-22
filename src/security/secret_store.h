#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace security {

struct EncryptionEnvelope {
    std::vector<unsigned char> salt;
    std::vector<unsigned char> iv;
    std::vector<unsigned char> ciphertext;
    std::vector<unsigned char> tag;
};

// SecretStore wraps a small, encrypted configuration store.
//
// API keys and other credentials are kept encrypted on disk using
// AES-256-GCM. The master password is stretched with PBKDF2 before
// being used as the symmetric key. All sensitive buffers are erased
// in the destructor to minimise memory exposure.
class SecretStore {
  public:
    explicit SecretStore(std::string master_password);
    ~SecretStore();

    // Loads the encrypted payload located at |path|. The format is a
    // simple key-value listing that has been encrypted and base64 encoded.
    // Throws std::runtime_error on failure.
    void load(const std::filesystem::path &path);

    // Persists the current secret map to |path|. An exception is thrown if
    // the payload cannot be encrypted or written.
    void save(const std::filesystem::path &path) const;

    // Inserts or updates a secret value in memory.
    void set_secret(const std::string &key, const std::string &value);

    // Returns a secret value when present, otherwise std::nullopt.
    [[nodiscard]] std::optional<std::string> get_secret(const std::string &key) const;

    // Provides a stable view of the known secret identifiers. This is useful
    // for higher-level diagnostics and key rotation tooling.
    [[nodiscard]] std::vector<std::string> list_keys() const;

    // Removes a secret from the in-memory store. The change will be persisted
    // on the next call to save().
    void erase_secret(const std::string &key);

  private:
    [[nodiscard]] EncryptionEnvelope encrypt_payload(const std::string &plaintext) const;
    [[nodiscard]] std::string decrypt_payload(const EncryptionEnvelope &envelope) const;

    [[nodiscard]] std::vector<unsigned char> derive_key(const std::vector<unsigned char> &salt) const;

    static std::vector<unsigned char> random_bytes(std::size_t count);
    static std::string serialize(const std::unordered_map<std::string, std::string> &payload);
    static std::unordered_map<std::string, std::string> deserialize(const std::string &payload);

    static std::string base64_encode(const std::vector<unsigned char> &input);
    static std::vector<unsigned char> base64_decode(const std::string &input);

    std::string master_password_;
    std::unordered_map<std::string, std::string> secrets_;
};

}  // namespace security
