#include "WindowsRetailDataSelection.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "windows retail data selection smoke failed: %s\n",
               message);
  return 1;
}

std::wstring Join(const std::wstring& base, const wchar_t* child) {
  return base + L"\\" + child;
}

bool WriteRaw(const std::wstring& path, const std::string& bytes) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0u;
  const bool result = WriteFile(file, bytes.data(),
                                static_cast<DWORD>(bytes.size()), &written,
                                nullptr) != FALSE &&
                      written == bytes.size();
  CloseHandle(file);
  return result;
}

bool ReadRaw(const std::wstring& path, std::string* bytes) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
      size.QuadPart > 65536) {
    CloseHandle(file);
    return false;
  }
  bytes->assign(static_cast<std::size_t>(size.QuadPart), '\0');
  DWORD read = 0u;
  const bool result = bytes->empty() ||
      (ReadFile(file, &(*bytes)[0], static_cast<DWORD>(bytes->size()), &read,
                nullptr) != FALSE && read == bytes->size());
  CloseHandle(file);
  return result;
}

}  // namespace

int wmain() {
  wchar_t temporary[MAX_PATH] = {};
  if (GetTempPathW(MAX_PATH, temporary) == 0u) return Fail("no temp path");
  wchar_t name[96] = {};
  std::swprintf(name, sizeof(name) / sizeof(name[0]),
                L"rr2nw-data-selection-%lu", GetCurrentProcessId());
  const std::wstring root = Join(temporary, name);
  RemoveDirectoryW(root.c_str());
  if (!CreateDirectoryW(root.c_str(), nullptr)) return Fail("no scratch root");

  const std::wstring file = Join(root, L"retail-data.cfg");
  const std::wstring first = L"C:\\RR2NW Retail Data";
  const std::wstring second = L"D:\\Games\\The Next Worlds";
  std::wstring decoded;
  std::string detail;
  if (rr2nw::WindowsRetailDataSelection_Read(file, &decoded, &detail) !=
      rr2nw::WindowsRetailDataSelectionReadResult::kMissing)
    return Fail("missing state diverged");
  if (!rr2nw::WindowsRetailDataSelection_WriteAtomic(file, first, &detail) ||
      rr2nw::WindowsRetailDataSelection_Read(file, &decoded, &detail) !=
          rr2nw::WindowsRetailDataSelectionReadResult::kReady ||
      decoded != first)
    return Fail("valid round-trip diverged");
  std::string original;
  if (!ReadRaw(file, &original) || original.find("C:\\") != std::string::npos)
    return Fail("selection is not encoded");
  if (!rr2nw::WindowsRetailDataSelection_WriteAtomic(file, first, &detail))
    return Fail("idempotent write failed");
  std::string repeated;
  if (!ReadRaw(file, &repeated) || repeated != original)
    return Fail("idempotent bytes diverged");

  const std::wstring tempBlocker = file + L".tmp";
  if (!CreateDirectoryW(tempBlocker.c_str(), nullptr))
    return Fail("could not create write-failure seam");
  if (rr2nw::WindowsRetailDataSelection_WriteAtomic(file, second, &detail))
    return Fail("blocked atomic write succeeded");
  if (rr2nw::WindowsRetailDataSelection_Read(file, &decoded, &detail) !=
          rr2nw::WindowsRetailDataSelectionReadResult::kReady ||
      decoded != first)
    return Fail("failed write mutated committed state");
  RemoveDirectoryW(tempBlocker.c_str());

  const std::vector<std::string> corrupt = {
      "", "RR2DATA2\nversion=1\npath_hex=433a5c78\n",
      "RR2DATA1\nversion=2\npath_hex=433a5c78\n",
      "RR2DATA1\nversion=1\npath_hex=0\n",
      "RR2DATA1\nversion=1\npath_hex=ff\n",
      "RR2DATA1\nversion=1\npath_hex=72656c6174697665\n",
      std::string(8193u, 'x')};
  for (const std::string& fixture : corrupt) {
    if (!WriteRaw(file, fixture) ||
        rr2nw::WindowsRetailDataSelection_Read(file, &decoded, &detail) !=
            rr2nw::WindowsRetailDataSelectionReadResult::kCorrupt)
      return Fail("corrupt fixture was admitted");
  }
  if (rr2nw::WindowsRetailDataSelection_WriteAtomic(
          file, L"relative\\data", &detail))
    return Fail("relative path was admitted");

  DeleteFileW(file.c_str());
  RemoveDirectoryW(root.c_str());
  std::printf("windows retail data selection smoke: missing=1 roundtrip=1 "
              "atomic=1 corrupt=%zu\n", corrupt.size());
  return 0;
}
