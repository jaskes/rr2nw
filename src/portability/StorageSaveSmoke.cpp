#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "storage/h/savefile.h"

namespace {

constexpr int kExpectedLevel = 7;
constexpr int kExpectedNumber = -1999;
constexpr double kExpectedValue = 27.05;
constexpr char kExpectedText[] = "storage-state";

int Fail(const char* message) {
  std::cerr << "legacy-storage-save-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool WriteFixture(const char* path) {
  PIN_SaveFile file;
  file.m_Level = kExpectedLevel;
  int number = kExpectedNumber;
  double value = kExpectedValue;
  char text[] = "storage-state";
  if (!file.OpenWrite(const_cast<char*>(path)) ||
      !file.WriteData(reinterpret_cast<char*>(&number), sizeof(number)) ||
      !file.WriteData(reinterpret_cast<char*>(&value), sizeof(value)) ||
      !file.WriteData(text, sizeof(text))) {
    file.Close();
    return false;
  }
  file.Close();
  return true;
}

bool ReadFixture(const char* path) {
  PIN_SaveFile file;
  int number = 0;
  double value = 0.0;
  char text[sizeof(kExpectedText)] = {};
  if (!file.OpenRead(const_cast<char*>(path)) ||
      file.m_Level != kExpectedLevel ||
      !file.GetData(reinterpret_cast<char*>(&number), sizeof(number)) ||
      !file.GetData(reinterpret_cast<char*>(&value), sizeof(value)) ||
      !file.GetData(text, sizeof(text))) {
    file.Close();
    return false;
  }

  const bool valid = number == kExpectedNumber && value == kExpectedValue &&
                     std::strcmp(text, kExpectedText) == 0 &&
                     file.GetCurrentData() == nullptr && !file.Shift(1);
  file.Close();
  return valid;
}

bool MakeTruncatedCopy(const char* source, const char* destination) {
  std::ifstream input(source, std::ios::binary);
  const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
  if (input.bad() || bytes.size() < 2) {
    return false;
  }

  std::ofstream output(destination, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size() - 1));
  return output.good();
}

bool RejectTruncatedFixture(const char* path) {
  PIN_SaveFile file;
  int number = 0;
  double value = 0.0;
  char text[sizeof(kExpectedText)] = {};
  if (!file.OpenRead(const_cast<char*>(path)) ||
      !file.GetData(reinterpret_cast<char*>(&number), sizeof(number)) ||
      !file.GetData(reinterpret_cast<char*>(&value), sizeof(value))) {
    file.Close();
    return false;
  }

  const bool rejected = !file.GetData(text, sizeof(text));
  file.Close();
  return rejected;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4, "storage contract requires Win32");
  static_assert(sizeof(long) == 4, "save fields require 32-bit long");

  if (argc != 2) {
    return Fail("expected a temporary save path");
  }

  const std::string fixture_path(argv[1]);
  const std::string truncated_path = fixture_path + ".truncated";
  std::remove(fixture_path.c_str());
  std::remove(truncated_path.c_str());

  if (!WriteFixture(fixture_path.c_str())) {
    return Fail("could not write fixture");
  }
  if (!SetFileAttributesA(fixture_path.c_str(), FILE_ATTRIBUTE_READONLY)) {
    return Fail("could not make fixture read-only");
  }
  const bool read_only_round_trip = ReadFixture(fixture_path.c_str());
  const bool restored_attributes =
      SetFileAttributesA(fixture_path.c_str(), FILE_ATTRIBUTE_NORMAL) != 0;
  if (!read_only_round_trip || !restored_attributes) {
    return Fail("save round-trip changed state or final-boundary behavior");
  }
  if (!MakeTruncatedCopy(fixture_path.c_str(), truncated_path.c_str())) {
    return Fail("could not create truncated fixture");
  }
  if (!RejectTruncatedFixture(truncated_path.c_str())) {
    return Fail("truncated save payload was not rejected cleanly");
  }

  std::remove(fixture_path.c_str());
  std::remove(truncated_path.c_str());
  std::cout << "legacy-storage-save-smoke: OK\n";
  return EXIT_SUCCESS;
}
