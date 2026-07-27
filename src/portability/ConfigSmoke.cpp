#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "filesys.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-config-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool WriteFixture(const char* path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << "# controlled Vehicle config ";
  // Retail configs contain legacy single-byte Cyrillic comments. Keep bytes
  // above 0x7f in the regression fixture so ctype calls cannot regress to
  // signed-char undefined behavior.
  output.put(static_cast<char>(0xac));
  output.put(static_cast<char>(0xa8));
  output.put(static_cast<char>(0xe0));
  output << "\r\n"
            "[Vehicle]\r\n"
            "Type=Emveshka\r\n"
            "Wheels=4\r\n"
            "Mass=12.5\r\n";
  return output.good();
}

bool ReadFixture(const char* path) {
  CConfigFile config(path, true);
  const char* type = config("Vehicle", "Type");
  return type != nullptr && std::strcmp(type, "Emveshka") == 0 &&
         config.GetInt("Vehicle", "Wheels", -1) == 4 &&
         config.GetDouble("Vehicle", "Mass", -1.0) == 12.5 &&
         config.GetInt("Vehicle", "Missing", 17) == 17;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    return Fail("expected a temporary config path");
  }

  std::remove(argv[1]);
  if (!WriteFixture(argv[1])) {
    return Fail("could not write config fixture");
  }
  if (!ReadFixture(argv[1])) {
    return Fail("config parsing diverged");
  }

  std::remove(argv[1]);
  std::cout << "legacy-config-smoke: OK\n";
  return EXIT_SUCCESS;
}
