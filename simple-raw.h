#pragma once

#include <string.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <limits>

[[nodiscard]] size_t SafeRead(int fd, char* ptr, size_t size);
[[nodiscard]] size_t SafeWrite(int fd, const char* ptr, size_t size);

uint32_t ReadBinary(int in, uint32_t defaultValue, bool& good);
uint64_t ReadBinary(int in, uint64_t defaultValue, bool& good);
std::string ReadBinary(int in, const std::string& defaultValue, bool& good);
template <typename T>
std::vector<T> ReadBinary(int in, const std::vector<T>& defaultValue, bool& good) {
  uint32_t size = ReadBinary(in, uint32_t(0), good);
  std::vector<T> retval;
  for (size_t i = 0; good && i < size; ++i) {
    retval.push_back(ReadBinary(in, T()));
  }
  return good ? retval : defaultValue;
}
template <typename K, typename V>
std::map<K,V> ReadBinary(int in, const std::map<K, V>& defaultValue, bool& good) {
  uint32_t size = ReadBinary(in, uint32_t(0), good);
  std::map<K,V> retval;
  for (size_t i = 0; good && i < size; ++i) {
    auto k = ReadBinary(in, K());
    auto v = ReadBinary(in, V());
    retval[std::move(k)] = std::move(v);
  }
  return good ? retval : defaultValue;
}

[[nodiscard]] bool WriteBinary(int out, uint32_t number);
[[nodiscard]] bool WriteBinary(int out, uint64_t number);
[[nodiscard]] bool WriteBinary(int out, const char* data, uint32_t size);
[[nodiscard]] inline bool WriteBinary(int out, const char* str) {
	auto len = str ? strlen(str) : 0;
	return WriteBinary(out, str, len < std::numeric_limits<uint32_t>::max() ? uint32_t(len) : 0);
}
[[nodiscard]] inline bool WriteBinary(int out, const std::string& str) {
	return WriteBinary(out, str.c_str(), str.length() < std::numeric_limits<uint32_t>::max() ? uint32_t(str.length()) : 0);
}
template <typename T>
[[nodiscard]] bool WriteBinary(int out, const std::vector<T>& vector) {
  if (!WriteBinary(out, uint32_t(vector.size())))
    return false;
  for (const auto& e : vector)
    if (!WriteBinary(out, e))
      return false;
  return true;
}
template <typename K, typename V>
[[nodiscard]] bool WriteBinary(int out, const std::map<K, V>& map) {
  if (!WriteBinary(out, uint32_t(map.size())))
    return false;
  for (const auto& e : map) {
    if (!WriteBinary(out, e.first))
      return false;
    if (!WriteBinary(out, e.second))
      return false;
  }
  return true;
}
