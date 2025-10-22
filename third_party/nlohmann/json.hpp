#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nlohmann {

class json {
 public:
  class exception : public std::exception {
   public:
    explicit exception(std::string message) noexcept : message_(std::move(message)) {}

    const char* what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_;
  };

  json() = default;

  static json object() {
    json j;
    j.type_ = Type::Object;
    return j;
  }

  static json array() {
    json j;
    j.type_ = Type::Array;
    return j;
  }

  static json parse(const std::string& text) {
    std::size_t pos = 0;
    json value = parseValue(text, pos);
    skipWhitespace(text, pos);
    if (pos != text.size()) {
      throw exception("Unexpected trailing characters in JSON");
    }
    return value;
  }

  bool contains(const std::string& key) const {
    return is_object() && object_.find(key) != object_.end();
  }

  json operator[](const std::string& key) const {
    if (!contains(key)) {
      return json();
    }
    return object_.at(key);
  }

  bool is_object() const { return type_ == Type::Object; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_string() const { return type_ == Type::String; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_boolean() const { return type_ == Type::Boolean; }

  bool empty() const {
    if (is_object()) {
      return object_.empty();
    }
    if (is_array()) {
      return array_.empty();
    }
    if (is_string()) {
      return string_.empty();
    }
    return true;
  }

  std::size_t size() const {
    if (is_object()) {
      return object_.size();
    }
    if (is_array()) {
      return array_.size();
    }
    return 0;
  }

  json front() const {
    if (is_array() && !array_.empty()) {
      return array_.front();
    }
    return json();
  }

  template <typename T>
  T value(const std::string& key, T default_value) const {
    if (!contains(key)) {
      return default_value;
    }
    return object_.at(key).get(default_value);
  }

  std::string value(const std::string& key, const char* default_value) const {
    return value(key, std::string(default_value));
  }

  std::vector<json>::const_iterator begin() const { return array_.begin(); }
  std::vector<json>::const_iterator end() const { return array_.end(); }

 private:
  enum class Type { Null, Object, Array, String, Number, Boolean };

  template <typename T>
  T get(T default_value) const {
    if constexpr (std::is_same<T, std::string>::value) {
      return is_string() ? string_ : default_value;
    } else if constexpr (std::is_integral<T>::value) {
      if (is_number()) {
        return static_cast<T>(number_);
      }
      return default_value;
    } else if constexpr (std::is_floating_point<T>::value) {
      if (is_number()) {
        return static_cast<T>(number_);
      }
      return default_value;
    } else if constexpr (std::is_same<T, json>::value) {
      return *this;
    } else {
      return default_value;
    }
  }

  static void skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
      ++pos;
    }
  }

  static json parseValue(const std::string& text, std::size_t& pos) {
    skipWhitespace(text, pos);
    if (pos >= text.size()) {
      throw exception("Unexpected end of JSON");
    }

    char ch = text[pos];
    if (ch == '{') {
      return parseObject(text, pos);
    }
    if (ch == '[') {
      return parseArray(text, pos);
    }
    if (ch == '"') {
      return parseString(text, pos);
    }
    if (ch == 't' || ch == 'f' || ch == 'n') {
      return parseLiteral(text, pos);
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      return parseNumber(text, pos);
    }

    throw exception("Invalid JSON value");
  }

  static json parseObject(const std::string& text, std::size_t& pos) {
    json result = json::object();
    ++pos;  // consume '{'
    skipWhitespace(text, pos);
    if (pos < text.size() && text[pos] == '}') {
      ++pos;
      return result;
    }

    while (pos < text.size()) {
      json key_json = parseString(text, pos);
      std::string key = key_json.string_;

      skipWhitespace(text, pos);
      if (pos >= text.size() || text[pos] != ':') {
        throw exception("Expected ':' in object");
      }
      ++pos;  // consume ':'

      json value = parseValue(text, pos);
      result.object_.emplace(std::move(key), std::move(value));

      skipWhitespace(text, pos);
      if (pos >= text.size()) {
        throw exception("Unexpected end of object");
      }
      if (text[pos] == '}') {
        ++pos;
        break;
      }
      if (text[pos] != ',') {
        throw exception("Expected ',' in object");
      }
      ++pos;
      skipWhitespace(text, pos);
    }

    return result;
  }

  static json parseArray(const std::string& text, std::size_t& pos) {
    json result = json::array();
    ++pos;  // consume '['
    skipWhitespace(text, pos);
    if (pos < text.size() && text[pos] == ']') {
      ++pos;
      return result;
    }

    while (pos < text.size()) {
      result.array_.push_back(parseValue(text, pos));
      skipWhitespace(text, pos);
      if (pos >= text.size()) {
        throw exception("Unexpected end of array");
      }
      if (text[pos] == ']') {
        ++pos;
        break;
      }
      if (text[pos] != ',') {
        throw exception("Expected ',' in array");
      }
      ++pos;
      skipWhitespace(text, pos);
    }

    return result;
  }

  static json parseString(const std::string& text, std::size_t& pos) {
    if (text[pos] != '"') {
      throw exception("Expected string");
    }
    ++pos;
    std::string value;
    while (pos < text.size()) {
      char ch = text[pos++];
      if (ch == '"') {
        json result;
        result.type_ = Type::String;
        result.string_ = std::move(value);
        return result;
      }
      if (ch == '\\') {
        if (pos >= text.size()) {
          throw exception("Invalid escape sequence");
        }
        char esc = text[pos++];
        switch (esc) {
          case '"':
          case '\\':
          case '/':
            value.push_back(esc);
            break;
          case 'b':
            value.push_back('\b');
            break;
          case 'f':
            value.push_back('\f');
            break;
          case 'n':
            value.push_back('\n');
            break;
          case 'r':
            value.push_back('\r');
            break;
          case 't':
            value.push_back('\t');
            break;
          default:
            throw exception("Unsupported escape sequence");
        }
      } else {
        value.push_back(ch);
      }
    }
    throw exception("Unterminated string");
  }

  static json parseNumber(const std::string& text, std::size_t& pos) {
    std::size_t start = pos;
    if (text[pos] == '-') {
      ++pos;
    }
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
      ++pos;
    }
    if (pos < text.size() && text[pos] == '.') {
      ++pos;
      while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
      }
    }
    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
      ++pos;
      if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
        ++pos;
      }
      while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
      }
    }
    double value = std::stod(text.substr(start, pos - start));
    json result;
    result.type_ = Type::Number;
    result.number_ = value;
    return result;
  }

  static json parseLiteral(const std::string& text, std::size_t& pos) {
    if (text.compare(pos, 4, "true") == 0) {
      pos += 4;
      json result;
      result.type_ = Type::Boolean;
      result.boolean_ = true;
      return result;
    }
    if (text.compare(pos, 5, "false") == 0) {
      pos += 5;
      json result;
      result.type_ = Type::Boolean;
      result.boolean_ = false;
      return result;
    }
    if (text.compare(pos, 4, "null") == 0) {
      pos += 4;
      return json();
    }
    throw exception("Invalid literal");
  }

  Type type_{Type::Null};
  std::map<std::string, json> object_;
  std::vector<json> array_;
  std::string string_;
  double number_{0.0};
  bool boolean_{false};
};

}  // namespace nlohmann
