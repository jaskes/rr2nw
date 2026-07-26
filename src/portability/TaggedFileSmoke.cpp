#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "filesys.h"

namespace {

constexpr int kRootVersion = 3;
constexpr int kMetadataVersion = 2;
constexpr int kDataVersion = 1;
constexpr int kExpectedNumber = -1999;
constexpr dword kExpectedFlags = 0x5242324eUL;
constexpr double kExpectedValue = 27.05;

int Fail(const char* message) {
  std::cerr << "legacy-tagged-file-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool WriteFixture(const char* path) {
  CTaggedFile file(false);
  if (!file.Create(path, false)) {
    return false;
  }
  if (file.Descend("ROOT", false, kRootVersion, false) < 0 ||
      file.Descend("metadata", true, kMetadataVersion, false) < 0 ||
      !file.WriteInt(kExpectedNumber) || !file.WriteDWord(kExpectedFlags) ||
      !file.Ascend() ||
      file.Descend("DATA", true, kDataVersion, false) < 0 ||
      !file.WriteDouble(kExpectedValue) || !file.Ascend() || !file.Ascend()) {
    file.Close(false);
    return false;
  }
  return file.Close();
}

bool ReadFixture(const char* path) {
  CTaggedFile file(false);
  if (!file.Open(path, false) ||
      file.Descend("ROOT", false, kRootVersion, false) != kRootVersion ||
      file.Descend("metadata", true, kMetadataVersion, false) !=
          kMetadataVersion) {
    if (file.Level() > 0) {
      file.Close(false);
    }
    return false;
  }

  int number = 0;
  dword flags = 0;
  double value = 0.0;
  const bool valid = file.ReadInt(number) && file.ReadDWord(flags) &&
                     file.BytesLeft() == 0 && file.Ascend() &&
                     file.Descend("DATA", true, kDataVersion, false) ==
                         kDataVersion &&
                     file.ReadDouble(value) && file.BytesLeft() == 0 &&
                     file.Ascend() && file.Ascend() && file.Close();
  return valid && number == kExpectedNumber && flags == kExpectedFlags &&
         value == kExpectedValue;
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
  CTaggedFile file(false);
  if (!file.Open(path, false) ||
      file.Descend("ROOT", false, kRootVersion, false) != kRootVersion ||
      file.Descend("metadata", true, kMetadataVersion, false) !=
          kMetadataVersion) {
    if (file.Level() > 0) {
      file.Close(false);
    }
    return false;
  }

  int number = 0;
  dword flags = 0;
  double value = 0.0;
  if (!file.ReadInt(number) || !file.ReadDWord(flags) || !file.Ascend() ||
      file.Descend("DATA", true, kDataVersion, false) != kDataVersion) {
    file.Close(false);
    return false;
  }

  const bool rejected = !file.ReadDouble(value) && !file.IsOK();
  file.Close(false);
  return rejected;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(long) == 4, "tagged-file format requires 32-bit long");
  static_assert(sizeof(dword) == 4, "tagged-file format requires 32-bit dword");

  if (argc != 2) {
    return Fail("expected a temporary fixture path");
  }

  const std::string fixture_path(argv[1]);
  const std::string truncated_path = fixture_path + ".truncated";
  std::remove(fixture_path.c_str());
  std::remove(truncated_path.c_str());

  if (!WriteFixture(fixture_path.c_str())) {
    return Fail("could not write nested fixture");
  }
  if (!ReadFixture(fixture_path.c_str())) {
    return Fail("round-trip changed a tagged-file value or layout");
  }
  if (!MakeTruncatedCopy(fixture_path.c_str(), truncated_path.c_str())) {
    return Fail("could not create a truncated fixture");
  }
  if (!RejectTruncatedFixture(truncated_path.c_str())) {
    return Fail("truncated terminal payload was not rejected cleanly");
  }

  std::remove(fixture_path.c_str());
  std::remove(truncated_path.c_str());
  std::cout << "legacy-tagged-file-smoke: OK\n";
  return EXIT_SUCCESS;
}
