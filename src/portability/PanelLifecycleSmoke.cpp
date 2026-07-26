#include "graph.h"
#include "sd1_epal.h"

#include <algorithm>
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

struct FixedFontHeader {
  char id[4];
  std::int32_t width;
  std::int32_t height;
  std::uint8_t palette[768];
  std::int32_t glyph_table[256 * 2];
  std::int32_t sprite_size;
};

static_assert(sizeof(PanelHeader) == 776, "panel fixture header drifted");
static_assert(sizeof(PanelResolution) == 44,
              "panel fixture resolution drifted");
static_assert(sizeof(FixedFontHeader) == 2832,
              "fixed-font fixture header drifted");

bool Expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << '\n';
  return false;
}

template <typename T>
void Write(std::ofstream& output, const T& value) {
  output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool WritePanelFixture(const std::string& path, bool truncate,
                       bool invalid_software_origin = false) {
  PanelHeader header = {{'P', 'N', 'L'}, 0, 1, {}};
  for (int color = 0; color < 256; ++color) {
    header.palette[color * 3] = static_cast<std::uint8_t>(color);
    header.palette[color * 3 + 1] = static_cast<std::uint8_t>(color);
    header.palette[color * 3 + 2] = static_cast<std::uint8_t>(color);
  }

  std::array<std::uint8_t, 16> software = {
      3, 0, 0, 0, 4, 0, 0, 0,
      2, 0x80, 5, 6, 3, 0, 0, 0};
  if (invalid_software_origin) {
    const std::int32_t invalid_x = 320;
    std::memcpy(software.data(), &invalid_x, sizeof(invalid_x));
  }
  PanelResolution resolution = {
      320, 200, 160, 90, 0, 0, 320, 180,
      static_cast<std::int32_t>(software.size()), 0, 0};

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  Write(output, header);
  Write(output, resolution);
  if (truncate) return true;
  output.write(reinterpret_cast<const char*>(software.data()), software.size());

  const std::int32_t control_count = 2;
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

  control.fill(0);
  std::memcpy(control.data(), "sector", 7);
  const std::int32_t indicator_type = 0x200;
  const std::int32_t indicator_x = 20;
  const std::int32_t indicator_y = 20;
  const float indicator_radius = 6.0f;
  const float indicator_start = 0.0f;
  const float indicator_finish = 1.57079632679f;
  const std::uint32_t indicator_color0 = 0x00323232;
  const std::uint32_t indicator_color1 = 0x003C3C3C;
  std::memcpy(control.data() + 40, &indicator_type, sizeof(indicator_type));
  std::memcpy(control.data() + 48, &indicator_x, sizeof(indicator_x));
  std::memcpy(control.data() + 52, &indicator_y, sizeof(indicator_y));
  std::memcpy(control.data() + 56, &indicator_radius,
              sizeof(indicator_radius));
  std::memcpy(control.data() + 60, &indicator_start,
              sizeof(indicator_start));
  std::memcpy(control.data() + 64, &indicator_finish,
              sizeof(indicator_finish));
  std::memcpy(control.data() + 68, &indicator_color0,
              sizeof(indicator_color0));
  std::memcpy(control.data() + 72, &indicator_color1,
              sizeof(indicator_color1));
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
    CGRImage sprite(2, 2);
    std::array<unsigned char, 4> pixels = {0, 7, 8, 9};
    sprite.SetPalette(_currPalette);
    sprite.LoadPalImage(pixels.data(), 0, 0, 0, 0, 1, 1, 2);
    std::fill(screen.begin(), screen.end(), static_cast<unsigned char>(1));
    if (!Expect(sprite.DrawSprite(5, 5) == 1,
                "software sprite did not draw") ||
        !Expect(screen[5 * 320 + 5] == 1 && screen[5 * 320 + 6] == 7 &&
                    screen[6 * 320 + 5] == 8 && screen[6 * 320 + 6] == 9,
                "software sprite transparency changed") ||
        !Expect(sprite.DrawSprite(-1, 8) == 1 &&
                    screen[8 * 320] == 7 && screen[9 * 320] == 9,
                "software sprite clipping changed")) return 1;
  }

  {
    FixedFontHeader header = {};
    std::memcpy(header.id, "FIXF", 4);
    header.width = 1;
    header.height = 1;
    for (int color = 0; color < 256; ++color) {
      header.palette[color * 3] = static_cast<std::uint8_t>(color);
      header.palette[color * 3 + 1] = static_cast<std::uint8_t>(color);
      header.palette[color * 3 + 2] = static_cast<std::uint8_t>(color);
    }
    header.glyph_table['0' * 2] = 1;
    header.sprite_size = 1;
    std::vector<unsigned char> font_data(sizeof(header) + 1);
    std::memcpy(font_data.data(), &header, sizeof(header));
    font_data.back() = 77;
    CFixedColorFont font;
    std::fill(screen.begin(), screen.end(), static_cast<unsigned char>(1));
    if (!Expect(font.ReadFromMemory(font_data.data()) == 1,
                "software font did not load") ||
        !Expect(font.PrintAt(3, 3, "0") == 1 && screen[3 * 320 + 3] == 77,
                "software font glyph changed")) return 1;
  }

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

  if (!WritePanelFixture(argv[2], false, true)) return 2;
  {
    CGRPanel invalid_origin(argv[2]);
    invalid_origin.SetResolution(320, 200);
    if (!Expect(invalid_origin.Open() == NULL,
                "out-of-range software-panel origin was accepted")) return 1;
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

    std::fill(screen.begin(), screen.end(), static_cast<unsigned char>(1));
    panel.EnableDrawPanel(0);
    panel.Draw();
    if (!Expect(screen[4 * 320 + 3] == 1,
                "disabled panel changed the framebuffer")) return 1;
    panel.EnableDrawPanel(1);
    panel.Draw();
    if (!Expect(screen[4 * 320 + 3] == 5 &&
                    screen[4 * 320 + 4] == 6,
                "literal panel run changed") ||
        !Expect(screen[4 * 320 + 5] == 1 &&
                    screen[4 * 320 + 6] == 1 &&
                    screen[4 * 320 + 7] == 1,
                "transparent panel run changed") ||
        !Expect(screen[2] == 42 && screen[10] == 42,
                "arrow control endpoints changed") ||
        !Expect(screen[18 * 320 + 22] == 60,
                "indicator sector fill changed")) return 1;
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
    GRSetViewport(full);
    retail.Draw();
    retail.Close();
    _gr_nScreenWidth = 320;
    _gr_nScreenHeight = 240;
    screen.assign(320 * 240, 0);
    _gr_pScreen = screen.data();
    retail.SetResolution(320, 240);
    SGRViewport* half = retail.Open();
    if (!Expect(half != NULL,
                "retail 320x240 panel metadata did not load")) return 1;
    GRSetViewport(half);
    retail.Draw();
    retail.Close();
  }

  _gr_pScreen = NULL;
  _gr_nScreenWidth = 320;
  _gr_nScreenHeight = 200;
  _dL.currDevice = NULL;
  return 0;
}
