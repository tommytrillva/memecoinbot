#pragma once

#include <cstddef>
#include <exception>
#include <map>
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
  json(std::nullptr_t) {}
  json(const json&) = default;
  json(json&&) noexcept = default;
  json& operator=(const json&) = default;
  json& operator=(json&&) noexcept = default;

  static json object() { return json(); }

  static json parse(const std::string&) { return json(); }

  bool contains(const std::string&) const { return false; }

  json operator[](const std::string&) const { return json(); }
  json& operator[](const std::string&) { return *this; }

  bool is_object() const { return false; }
  bool is_array() const { return false; }
  bool is_null() const { return true; }

  bool empty() const { return true; }
  std::size_t size() const { return 0; }

  json front() const { return json(); }

  template <typename T>
  T value(const std::string&, T default_value) const {
    return default_value;
  }

  template <typename T>
  T value(const std::string&, const T& default_value) {
    return default_value;
  }

  std::vector<json>::const_iterator begin() const { return array_.begin(); }
  std::vector<json>::const_iterator end() const { return array_.end(); }

 private:
  std::vector<json> array_{};
};

}  // namespace nlohmann
