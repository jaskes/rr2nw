#include <cstdio>
#include <cstdlib>

#include "RecoveredWavMetadataCatalog.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "recovered-wav-metadata-catalog-smoke: %s\n",
               message);
  return EXIT_FAILURE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return Fail("expected a retail level directory");

  SRecoveredWavMetadataCatalog catalog = {};
  SRecoveredWavMetadataCatalogResult result = {};
  if (!RecoveredWavMetadataCatalog_Load(argv[1], &catalog, &result)) {
    std::fprintf(stderr, "wav catalog issue=%u error=%s\n", result.issues,
                 result.error);
    return Fail("WAV metadata catalog did not validate");
  }
  if (!RecoveredWavMetadataCatalog_IsKnown(&catalog) ||
      catalog.entryCount <= 0 || catalog.entryCount > catalog.capacity ||
      catalog.localMainSourceFingerprint == 0 ||
      catalog.loadWavSourceFingerprint == 0 || catalog.fingerprint == 0) {
    return Fail("WAV metadata catalog is not a bounded known roster");
  }

  int extendedCount = 0;
  for (int index = 0; index < catalog.entryCount; ++index) {
    if (catalog.entries[index].flags != 0) ++extendedCount;
  }
  std::printf("wav-metadata-catalog entries=%d/%d extended=%d "
              "localmain_fingerprint=%llu loadwav_fingerprint=%llu "
              "fingerprint=%llu\n",
              catalog.entryCount, catalog.capacity, extendedCount,
              catalog.localMainSourceFingerprint,
              catalog.loadWavSourceFingerprint, catalog.fingerprint);
  return EXIT_SUCCESS;
}
