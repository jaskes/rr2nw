#include "WindowsRetailDataSelection.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace rr2nw {
namespace {

constexpr std::size_t kMaximumFileBytes = 8192u;
constexpr std::size_t kMaximumUtf8PathBytes = 2048u;

bool Fail(std::string* detail, const char* message) {
  if (detail != nullptr) *detail = message;
  return false;
}

bool WideToUtf8Strict(const std::wstring& value, std::string* result) {
  if (value.empty()) return false;
  const int length = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
      static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0 || static_cast<std::size_t>(length) > kMaximumUtf8PathBytes)
    return false;
  result->assign(static_cast<std::size_t>(length), '\0');
  return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                             static_cast<int>(value.size()), &(*result)[0],
                             length, nullptr, nullptr) == length;
}

bool Utf8ToWideStrict(const std::string& value, std::wstring* result) {
  if (value.empty() || value.size() > kMaximumUtf8PathBytes) return false;
  const int length = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
      static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return false;
  result->assign(static_cast<std::size_t>(length), L'\0');
  return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                             static_cast<int>(value.size()), &(*result)[0],
                             length) == length;
}

std::wstring CanonicalAbsolutePath(const std::wstring& path) {
  if (path.empty()) return std::wstring();
  const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (length == 0u) return std::wstring();
  std::vector<wchar_t> buffer(static_cast<std::size_t>(length));
  if (GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr) == 0u)
    return std::wstring();
  std::wstring canonical(buffer.data());
  while (canonical.size() > 3u &&
         (canonical.back() == L'\\' || canonical.back() == L'/'))
    canonical.pop_back();
  return canonical;
}

bool IsCanonicalAbsolutePath(const std::wstring& path) {
  if (path.empty()) return false;
  for (wchar_t character : path) {
    if (character < 0x20) return false;
  }
  const std::wstring canonical = CanonicalAbsolutePath(path);
  return !canonical.empty() && _wcsicmp(canonical.c_str(), path.c_str()) == 0;
}

char HexDigit(unsigned int value) {
  return static_cast<char>(value < 10u ? '0' + value : 'a' + (value - 10u));
}

int HexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

std::string HexEncode(const std::string& value) {
  std::string encoded;
  encoded.reserve(value.size() * 2u);
  for (unsigned char byte : value) {
    encoded.push_back(HexDigit(byte >> 4u));
    encoded.push_back(HexDigit(byte & 0x0fu));
  }
  return encoded;
}

bool HexDecode(const std::string& encoded, std::string* value) {
  if (encoded.empty() || (encoded.size() & 1u) != 0u ||
      encoded.size() > kMaximumUtf8PathBytes * 2u)
    return false;
  value->clear();
  value->reserve(encoded.size() / 2u);
  for (std::size_t index = 0u; index < encoded.size(); index += 2u) {
    const int high = HexValue(encoded[index]);
    const int low = HexValue(encoded[index + 1u]);
    if (high < 0 || low < 0) return false;
    value->push_back(static_cast<char>((high << 4) | low));
  }
  return true;
}

bool ReadBoundedFile(const std::wstring& path, std::string* bytes,
                     DWORD* error) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    *error = GetLastError();
    return false;
  }
  LARGE_INTEGER size = {};
  const bool validSize = GetFileSizeEx(file, &size) != FALSE &&
                         size.QuadPart > 0 &&
                         size.QuadPart <=
                             static_cast<LONGLONG>(kMaximumFileBytes);
  if (!validSize) {
    CloseHandle(file);
    *error = ERROR_INVALID_DATA;
    return false;
  }
  bytes->assign(static_cast<std::size_t>(size.QuadPart), '\0');
  DWORD read = 0u;
  const BOOL result = ReadFile(file, &(*bytes)[0],
                               static_cast<DWORD>(bytes->size()), &read,
                               nullptr);
  CloseHandle(file);
  if (result == FALSE || read != bytes->size()) {
    *error = ERROR_READ_FAULT;
    return false;
  }
  *error = ERROR_SUCCESS;
  return true;
}

bool Parse(const std::string& bytes, std::wstring* directory) {
  std::vector<std::string> lines;
  std::size_t begin = 0u;
  while (begin <= bytes.size()) {
    const std::size_t end = bytes.find('\n', begin);
    std::string line = bytes.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() || end != std::string::npos) lines.push_back(line);
    if (end == std::string::npos) break;
    begin = end + 1u;
  }
  if (lines.size() != 3u || lines[0] != "RR2DATA1" ||
      lines[1] != "version=1" ||
      lines[2].compare(0u, 9u, "path_hex=") != 0)
    return false;
  std::string utf8;
  std::wstring decoded;
  if (!HexDecode(lines[2].substr(9u), &utf8) ||
      !Utf8ToWideStrict(utf8, &decoded) ||
      !IsCanonicalAbsolutePath(decoded))
    return false;
  *directory = decoded;
  return true;
}

}  // namespace

WindowsRetailDataSelectionReadResult WindowsRetailDataSelection_Read(
    const std::wstring& filePath, std::wstring* directory,
    std::string* detail) {
  if (directory == nullptr || filePath.empty()) {
    Fail(detail, "selection arguments are invalid");
    return WindowsRetailDataSelectionReadResult::kCorrupt;
  }
  directory->clear();
  std::string bytes;
  DWORD error = ERROR_SUCCESS;
  if (!ReadBoundedFile(filePath, &bytes, &error)) {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      if (detail != nullptr) *detail = "selection is missing";
      return WindowsRetailDataSelectionReadResult::kMissing;
    }
    if (error == ERROR_INVALID_DATA) {
      if (detail != nullptr) *detail = "selection is outside size bounds";
      return WindowsRetailDataSelectionReadResult::kCorrupt;
    }
    if (detail != nullptr) *detail = "selection could not be read";
    return WindowsRetailDataSelectionReadResult::kIoFailure;
  }
  if (!Parse(bytes, directory)) {
    if (detail != nullptr) *detail = "selection format is invalid";
    return WindowsRetailDataSelectionReadResult::kCorrupt;
  }
  if (detail != nullptr) *detail = "selection is valid";
  return WindowsRetailDataSelectionReadResult::kReady;
}

bool WindowsRetailDataSelection_WriteAtomic(const std::wstring& filePath,
                                            const std::wstring& directory,
                                            std::string* detail) {
  if (filePath.empty() || !IsCanonicalAbsolutePath(directory))
    return Fail(detail, "selection path is not canonical and absolute");
  std::string utf8;
  if (!WideToUtf8Strict(directory, &utf8))
    return Fail(detail, "selection path is not bounded UTF-8");
  const std::string bytes = "RR2DATA1\r\nversion=1\r\npath_hex=" +
                            HexEncode(utf8) + "\r\n";
  if (bytes.size() > kMaximumFileBytes)
    return Fail(detail, "selection payload is oversized");

  const std::wstring temporary = filePath + L".tmp";
  DeleteFileW(temporary.c_str());
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return Fail(detail, "selection temporary file could not be created");
  DWORD written = 0u;
  const bool stored =
      WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                &written, nullptr) != FALSE &&
      written == bytes.size() && FlushFileBuffers(file) != FALSE;
  CloseHandle(file);
  if (!stored ||
      MoveFileExW(temporary.c_str(), filePath.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
    DeleteFileW(temporary.c_str());
    return Fail(detail, "selection could not be committed atomically");
  }
  if (detail != nullptr) *detail = "selection committed";
  return true;
}

}  // namespace rr2nw
