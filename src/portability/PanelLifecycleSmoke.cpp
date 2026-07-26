#include "graph.h"
#include "sd1_epal.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

extern SDeviceList _dL;
extern TExtendedPalette _EPal;
extern unsigned char _currPalette[768];

namespace {

struct PanelHeader {
  char id[3];
  std::uint8_t version;
  std::int32_t resolution_count;
  std::uint8_t palette[768];
};

struct PanelResolution {
  std::int32_t width;
  std::int32_t height;
  std::int32_t origin_x;
  std::int32_t origin_y;
  std::int32_t clip_left;
  std::int32_t clip_top;
  std::int32_t clip_right;
  std::int32_t clip_bottom;
  std::int32_t software_size;
  std::int32_t hardware_size;
  std::int32_t hardware_texture_size;
};

static_assert(sizeof(PanelHeader) == 776, "panel fixture header drifted");
static_assert(sizeof(PanelResolution) == 44,
              "panel fixture resolution drifted");

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << '\n';
  return false;
}

template <typename T>
void Write(std::ofstream& output, const T& value) {
  output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool WritePanelFixture(const std::string& path, bool truncate) {
  PanelHeader header = {{'P', 'N', 'L'}, 0, 1, {}};
  for (int color = 0; color < 256; ++color) {
    header.palette[color * 3] = static_cast<std::uint8_t>(color);
    header.palette[color * 3 + 1] = static_cast<std::uint8_t>(color);
    header.palette[color * 3 + 2] = static_cast<std::uint8_t>(color);
  }

  const std::array<std::uint8_t, 16> software = {
      3, 0, 0, 0, 4, 0, 0, 0,
      2, 0x80, 5, 6, 3, 0, 0, 0};
  PanelResolution resolution = {
      320, 200, 160, 90, 0, 0, 320, 180,
      static_cast<std::int32_t>(software.size()), 0, 0};

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  Write(output, header);
  Write(output, resolution);
  if (truncate) return true;
  output.write(reinterpret_cast<const char*>(software.data()), software.size());

  const std::int32_t control_count = 1;
  Write(output, control_count);
  std::array<std::uint8_t, 100> control = {};
  std::memcpy(control.data(), "needle", 7);
  const std::int32_t type = 0x100;
  std::memcpy(control.data() + 40, &type, sizeof(type));
  const float radius = 10.0f;
  const float transparent_radius = 2.0f;
  const float start = -1.0f;
  const float finish = 1.0f;
  const std::uint32_t color = 0x002A2A2A;
  std::memcpy(control.data() + 56, &radius, sizeof(radius));
  std::memcpy(control.data() + 60, &transparent_radius,
              sizeof(transparent_radius));
  std::memcpy(control.data() + 64, &start, sizeof(start));
  std::memcpy(control.data() + 68, &finish, sizeof(finish));
  std::memcpy(control.data() + 72, &color, sizeof(color));
  output.write(reinterpret_cast<const char*>(control.data()), control.size());
  return output.good();
}

bool WriteInvalidCountFixture(const std::string& path) {
  PanelHeader header = {{'P', 'N', 'L'}, 0, 5, {}};
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  Write(output, header);
  return output.good();
}

void CountClipUpdate() {}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 && argc != 4) {
    std::cerr << "expected valid/truncated fixtures and optional retail panel\n";
    return 2;
  }
  if (!WritePanelFixture(argv[1], false) ||
      !WritePanelFixture(argv[2], true)) {
    std::cerr << "failed to write panel fixtures\n";
    return 2;
  }

  SDeviceDescr device = {};
  device.swHw = GR_SOFTWARE;
  device.textureFormat[NORMAL_TEXTURE_INDEX].rgbBitCount = 8;
  _dL.currDevice = &device;
  _pGRSetClipRect = CountClipUpdate;
  for (int color = 0; color < 256; ++color) {
    _currPalette[color * 3] = static_cast<unsigned char>(color);
    _currPalette[color * 3 + 1] = static_cast<unsigned char>(color);
    _currPalette[color * 3 + 2] = static_cast<unsigned char>(color);
  }
  epal_Load8BitPal(_EPal, _currPalette, 256);
  std::vector<unsigned char> screen(320 * 200);
  _gr_pScreen = screen.data();

  CGRImage empty_image;
  if (!Expect(empty_image.Width() == 0 && empty_image.Height() == 0,
              "empty image dimensions were not initialized")) return 1;

  {
    CGRPanel missing("panel-that-does-not-exist.pnl");
    missing.SetResolution(320, 200);
    if (!Expect(missing.Open() == NULL,
                "missing panel unexpectedly opened a viewport")) return 1;
  }

  {
    CGRPanel truncated(argv[2]);
    truncated.SetResolution(320, 200);
    if (!Expect(truncated.Open() == NULL,
                "truncated panel retained a partial resolution")) return 1;
  }

  if (!WriteInvalidCountFixture(argv[2])) return 2;
  {
    CGRPanel oversized(argv[2]);
    oversized.SetResolution(320, 200);
    if (!Expect(oversized.Open() == NULL,
                "oversized resolution table was accepted")) return 1;
  }

  device.swHw = GR_HARDWARE;
  {
    CGRPanel hardware(argv[1]);
    hardware.SetResolution(320, 200);
    if (!Expect(hardware.Open() == NULL,
                "software panel lifecycle accepted a hardware device")) {
      return 1;
    }
  }
  device.swHw = GR_SOFTWARE;

  {
    CGRPanel panel(argv[1]);
    device.swHw = GR_HARDWARE;
    panel.SetResolution(320, 200);
    if (!Expect(panel.Open() == NULL,
                "loaded software panel accepted a hardware switch")) {
      return 1;
    }
    device.swHw = GR_SOFTWARE;
  }

  {
    CGRPanel panel(argv[1]);
    panel.SetResolution(640, 480);
    if (!Expect(panel.Open() == NULL,
                "unknown resolution unexpectedly opened")) return 1;

    panel.SetResolution(320, 200);
    SGRViewport* viewport = panel.Open();
    if (!Expect(viewport != NULL, "valid software panel did not open") ||
        !Expect(viewport->x == 160 && viewport->y == 90,
                "panel viewport origin changed") ||
        !Expect(viewport->clipRect.left == -160 &&
                    viewport->clipRect.top == -90 &&
                    viewport->clipRect.right == 160 &&
                    viewport->clipRect.bottom == 90,
                "panel viewport clipping changed") ||
        !Expect(panel.Open() == viewport,
                "reopening an active panel changed its viewport") ||
        !Expect(!panel.IsDigitControl("needle"),
                "arrow control was classified as digits")) return 1;

    float halfway = 0.5f;
    std::uint32_t encoded = 0;
    std::memcpy(&encoded, &halfway, sizeof(encoded));
    panel.SetControlValue(const_cast<char*>("needle"), encoded);
    GRSetViewport(viewport);
    panel.Close();
  }

  if (!Expect(_gr_pYCache == NULL && _gr_pOrigin == NULL &&
                  GRGetViewport() == NULL,
              "panel destruction left active viewport state dangling")) {
    return 1;
  }

  {
    CGRPanel panel(argv[1]);
    panel.SetResolution(320, 200);
    SGRViewport* viewport = panel.Open();
    if (!Expect(viewport != NULL,
                "device-teardown panel did not open")) return 1;
    GRSetViewport(viewport);
    _dL.currDevice = NULL;
  }
  if (!Expect(_gr_pYCache == NULL && _gr_pOrigin == NULL &&
                  GRGetViewport() == NULL,
              "device teardown left active viewport state dangling")) {
    return 1;
  }
  _dL.currDevice = &device;

  if (argc == 4) {
    _gr_nScreenWidth = 640;
    _gr_nScreenHeight = 480;
    screen.assign(640 * 480, 0);
    _gr_pScreen = screen.data();
    CGRPanel retail(argv[3]);
    retail.SetResolution(640, 480);
    SGRViewport* full = retail.Open();
    if (!Expect(full != NULL,
                "retail 640x480 panel metadata did not load")) return 1;
    retail.Close();
    _gr_nScreenWidth = 320;
    _gr_nScreenHeight = 240;
    screen.assign(320 * 240, 0);
    _gr_pScreen = screen.data();
    retail.SetResolution(320, 240);
    SGRViewport* half = retail.Open();
    if (!Expect(half != NULL,
                "retail 320x240 panel metadata did not load")) return 1;
    retail.Close();
  }

  _gr_pScreen = NULL;
  _gr_nScreenWidth = 320;
  _gr_nScreenHeight = 200;
  _dL.currDevice = NULL;
  return 0;
}
