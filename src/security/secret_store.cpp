#include "secret_store.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace security {
namespace {
constexpr std::size_t kKeySize = 32;        // AES-256
constexpr std::size_t kIvSize = 12;         // Recommended IV size for GCM
constexpr std::size_t kTagSize = 16;        // 128-bit authentication tag
constexpr unsigned int kPbkdf2Iterations = 150000;

void secure_zero(void *data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return;
    }
#if defined(_WIN32)
    SecureZeroMemory(data, size);
#else
    std::memset(data, 0, size);
#endif
}

std::vector<unsigned char> read_binary(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open encrypted secret store: " + path.string());
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    const std::string contents = buffer.str();
    return std::vector<unsigned char>(contents.begin(), contents.end());
}

void write_binary(const std::filesystem::path &path, const std::vector<unsigned char> &buffer) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("Unable to persist encrypted secret store: " + path.string());
    }
    stream.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
}

}  // namespace

SecretStore::SecretStore(std::string master_password)
    : master_password_(std::move(master_password)) {}

SecretStore::~SecretStore() {
    secure_zero(master_password_.data(), master_password_.size());
    for (auto &kv : secrets_) {
        secure_zero(kv.second.data(), kv.second.size());
    }
}

void SecretStore::load(const std::filesystem::path &path) {
    const auto buffer = read_binary(path);
    std::stringstream stream(std::string(buffer.begin(), buffer.end()));

    std::string version_line;
    std::getline(stream, version_line);
    if (version_line != "version:1") {
        throw std::runtime_error("Unsupported encrypted payload version");
    }

    std::string salt_line;
    std::string iv_line;
    std::string data_line;
    std::string tag_line;

    if (!std::getline(stream, salt_line) || !std::getline(stream, iv_line) ||
        !std::getline(stream, data_line) || !std::getline(stream, tag_line)) {
        throw std::runtime_error("Encrypted payload truncated");
    }

    EncryptionEnvelope envelope;
    envelope.salt = parse_field("salt", salt_line);
    envelope.iv = parse_field("iv", iv_line);
    envelope.ciphertext = parse_field("data", data_line);
    envelope.tag = parse_field("tag", tag_line);

    const std::string plaintext = decrypt_payload(envelope);
    secrets_ = deserialize(plaintext);
}

void SecretStore::save(const std::filesystem::path &path) const {
    const std::string plaintext = serialize(secrets_);
    const EncryptionEnvelope envelope = encrypt_payload(plaintext);

    std::ostringstream stream;
    stream << "version:1\n";
    stream << "salt:" << base64_encode(envelope.salt) << "\n";
    stream << "iv:" << base64_encode(envelope.iv) << "\n";
    stream << "data:" << base64_encode(envelope.ciphertext) << "\n";
    stream << "tag:" << base64_encode(envelope.tag) << "\n";

    const std::string serialized = stream.str();
    write_binary(path, std::vector<unsigned char>(serialized.begin(), serialized.end()));
}

void SecretStore::set_secret(const std::string &key, const std::string &value) {
    secrets_[key] = value;
}

std::optional<std::string> SecretStore::get_secret(const std::string &key) const {
    const auto it = secrets_.find(key);
    if (it == secrets_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::string> SecretStore::list_keys() const {
    std::vector<std::string> keys;
    keys.reserve(secrets_.size());
    for (const auto &kv : secrets_) {
        keys.push_back(kv.first);
    }
    return keys;
}

void SecretStore::erase_secret(const std::string &key) {
    const auto it = secrets_.find(key);
    if (it != secrets_.end()) {
        secure_zero(it->second.data(), it->second.size());
        secrets_.erase(it);
    }
}

EncryptionEnvelope SecretStore::encrypt_payload(const std::string &plaintext) const {
    EncryptionEnvelope envelope;
    envelope.salt = random_bytes(kKeySize);
    envelope.iv = random_bytes(kIvSize);
    envelope.tag.resize(kTagSize);

    const auto key = derive_key(envelope.salt);

    std::vector<unsigned char> ciphertext(plaintext.size() + kTagSize);
    int len = 0;
    int ciphertext_len = 0;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        throw std::runtime_error("Failed to allocate cipher context");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(envelope.iv.size()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set IV length");
    }

    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), envelope.iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialise key/IV");
    }

    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char *>(plaintext.data()),
                          static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encryption failed");
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(envelope.tag.size()), envelope.tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Unable to retrieve GCM tag");
    }

    EVP_CIPHER_CTX_free(ctx);

    envelope.ciphertext = std::move(ciphertext);
    return envelope;
}

std::string SecretStore::decrypt_payload(const EncryptionEnvelope &envelope) const {
    const auto key = derive_key(envelope.salt);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        throw std::runtime_error("Failed to allocate cipher context");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(envelope.iv.size()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set IV length");
    }

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), envelope.iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialise key/IV");
    }

    std::vector<unsigned char> plaintext(envelope.ciphertext.size());
    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, envelope.ciphertext.data(),
                          static_cast<int>(envelope.ciphertext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Ciphertext authentication failed");
    }
    plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(envelope.tag.size()),
                            const_cast<unsigned char *>(envelope.tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to configure GCM tag");
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("GCM tag verification failed");
    }
    plaintext_len += len;
    plaintext.resize(plaintext_len);

    EVP_CIPHER_CTX_free(ctx);

    return std::string(plaintext.begin(), plaintext.end());
}

std::vector<unsigned char> SecretStore::derive_key(const std::vector<unsigned char> &salt) const {
    std::vector<unsigned char> key(kKeySize);
    if (PKCS5_PBKDF2_HMAC(master_password_.data(), static_cast<int>(master_password_.size()),
                          salt.data(), static_cast<int>(salt.size()), kPbkdf2Iterations, EVP_sha256(),
                          static_cast<int>(key.size()), key.data()) != 1) {
        throw std::runtime_error("PBKDF2 key derivation failed");
    }
    return key;
}

std::vector<unsigned char> SecretStore::random_bytes(std::size_t count) {
    std::vector<unsigned char> buffer(count);
    if (RAND_bytes(buffer.data(), static_cast<int>(buffer.size())) != 1) {
        throw std::runtime_error("Unable to collect secure random bytes");
    }
    return buffer;
}

std::string SecretStore::serialize(const std::unordered_map<std::string, std::string> &payload) {
    std::ostringstream builder;
    for (auto it = payload.begin(); it != payload.end(); ++it) {
        builder << it->first << '=' << it->second;
        if (std::next(it) != payload.end()) {
            builder << '\n';
        }
    }
    return builder.str();
}

std::unordered_map<std::string, std::string> SecretStore::deserialize(const std::string &payload) {
    std::unordered_map<std::string, std::string> result;
    std::istringstream stream(payload);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            throw std::runtime_error("Malformed secret entry: " + line);
        }
        result.emplace(line.substr(0, pos), line.substr(pos + 1));
    }
    return result;
}

std::vector<unsigned char> SecretStore::parse_field(const std::string &field_name, const std::string &line) {
    const auto delimiter = line.find(':');
    if (delimiter == std::string::npos) {
        throw std::runtime_error("Invalid field in encrypted payload: " + line);
    }
    const std::string key = line.substr(0, delimiter);
    if (key != field_name) {
        throw std::runtime_error("Unexpected field '" + key + "' (expected '" + field_name + "')");
    }
    const std::string encoded = line.substr(delimiter + 1);
    return base64_decode(encoded);
}

namespace {
const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode_impl(const std::vector<unsigned char> &input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    unsigned int val = 0;
    int bits = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            output.push_back(kBase64Alphabet[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }

    if (bits > -6) {
        output.push_back(kBase64Alphabet[((val << 8) >> (bits + 8)) & 0x3F]);
    }
    while (output.size() % 4 != 0) {
        output.push_back('=');
    }
    return output;
}

std::vector<unsigned char> base64_decode_impl(const std::string &input) {
    std::vector<int> t(256, -1);
    for (int i = 0; i < 64; ++i) {
        t[static_cast<unsigned char>(kBase64Alphabet[i])] = i;
    }

    std::vector<unsigned char> output;
    output.reserve((input.size() / 4) * 3);

    int val = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (t[c] == -1) {
            if (c == '=') {
                break;
            }
            throw std::runtime_error("Invalid character in base64 payload");
        }
        val = (val << 6) + t[c];
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<unsigned char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return output;
}

}  // namespace

std::string SecretStore::base64_encode(const std::vector<unsigned char> &input) {
    return base64_encode_impl(input);
}

std::vector<unsigned char> SecretStore::base64_decode(const std::string &input) {
    return base64_decode_impl(input);
}

}  // namespace security
