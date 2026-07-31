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
      "secondary_fire_interval": 0.45,
      "secondary_projectile": "Bullet.Mina",
      "damage_power": 7
    }],
    "projectiles": [{"id": "Bullet.Led.Prim", "speed": 180}],
    "people": [{
      "id": "peop.attr.man_c0",
      "movement_speed": 4.25,
      "initial_health": 0.8,
      "fire_interval": 0.35,
      "burst_count": 7
    }],
    "tanks": [{
      "id": "tank.attr.grasshopper",
      "max_speed": 22,
      "attack_power": 12,
      "attack_delay": 3.5
    }]
  })JSON";
  const char* projectileOnly =
      R"JSON({"schema":1,"projectiles":[{"id":"Bullet.Sec","speed":90.5}]})JSON";
  const char* secondaryOnly =
      R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","secondary_projectile":"Bullet.Sec","secondary_fire_interval":0.8}]})JSON";
  const char* peopleOnly =
      R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","movement_speed":3,"initial_health":1,"fire_interval":0.5,"burst_count":4}]})JSON";
  const char* tankOnly =
      R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper","max_speed":20,"attack_power":10,"attack_delay":2}]})JSON";
  if (!Valid(complete) || !Valid(projectileOnly) || !Valid(secondaryOnly) ||
      !Valid(peopleOnly) || !Valid(tankOnly) ||
      !Invalid(R"JSON({"schema":2,"vehicles":[{"id":"Vehicle.Attr.default","max_speed":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"extra":[],"vehicles":[{"id":"Vehicle.Attr.default","max_speed":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","mass":10}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","max_speed":0.1}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","turn_speed":721}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","secondary_fire_interval":0.01}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","secondary_projectile":""}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","secondary_projectile":"Bullet Secondary"}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"vehicles":[{"id":"Vehicle.Attr.default","secondary_projectile":"Bullet.Secondary.Identifier.That.Is.Too.Long"}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"projectiles":[{"id":"Bullet.Led.Prim","speed":2001}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0"}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","movement_speed":0.01}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","initial_health":0}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","fire_interval":11}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","burst_count":0}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","burst_count":1.5}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","armour":2}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"people":[{"id":"peop.attr.man_c0","movement_speed":3},{"id":"PEOP.ATTR.MAN_C0","movement_speed":4}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper"}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper","max_speed":101}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper","attack_power":0}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper","attack_delay":61}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper","cannon":"Cannon.Attr"}]})JSON") ||
      !Invalid(R"JSON({"schema":1,"tanks":[{"id":"tank.attr.grasshopper","max_speed":20},{"id":"TANK.ATTR.GRASSHOPPER","max_speed":21}]})JSON") ||
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
  std::printf("gameplay tuning schema=1 strict vehicle=8 projectile=1 "
              "people=4 tank=3\n");
  return EXIT_SUCCESS;
}
