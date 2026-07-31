#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "RecoveredGameplayTuningRuntime.h"

namespace {

bool Valid(const char* text) {
  char error[256] = {};
  return RecoveredGameplayTuning_ValidateText(
      text, std::strlen(text), error, sizeof(error));
}

bool Invalid(const char* text) { return !Valid(text); }

}  // namespace

int main() {
  const char* complete = R"JSON({
    "schema": 1,
    "vehicles": [{
      "id": "Vehicle.Attr.default",
      "max_speed": 14.0,
      "reverse_speed": 8,
      "acceleration_time": 5e-1,
      "turn_speed": 160,
      "primary_fire_interval": 0.12,
      "damage_power": 7
    }],
    "projectiles": [{"id": "Bullet.Led.Prim", "speed": 180}]
  })JSON";
  const char* projectileOnly =
      R"JSON({"schema":1,"projectiles":[{"id":"Bullet.Sec","speed":90.5}]})JSON";
  if (!Valid(complete) || !Valid(projectileOnly) ||
      !Invalid(R"JSON({"schema":2,"vehicles":[{"id":"Vehicle.Attr.default","max_speed":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"extra":[],"vehicles":[{"id":"Vehicle.Attr.default","max_speed":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","mass":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","max_speed":0.1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","turn_speed":721}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"projectiles":[{"id":"Bullet.Led.Prim","speed":2001}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","max_speed":10},{"id":"vehicle.attr.DEFAULT","max_speed":11}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","max_speed":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle Attr default","max_speed":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"projectiles":[{"id":"Bullet.Led.Prim","speed":NaN}]})JSON")) {
    std::fprintf(stderr, "strict gameplay tuning schema regression\n");
    return EXIT_FAILURE;
  }
  char error[32] = {};
  if (RecoveredGameplayTuning_ValidateText(nullptr, 0, error,
                                           sizeof(error)) ||
      error[0] == '\0') {
    std::fprintf(stderr, "invalid input did not produce bounded diagnostics\n");
    return EXIT_FAILURE;
  }
  std::printf("gameplay tuning schema=1 strict vehicle=6 projectile=1\n");
  return EXIT_SUCCESS;
}
