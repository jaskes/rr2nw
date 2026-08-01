#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "h/vehicle.h"
#include "storage/h/savefile.h"
#include "VehicleDeathCameraState.h"

namespace {

struct VehicleStaticFixture {
  bool dead;
  double current_time;
  int taking_taxi;
  double taxi_current_angle;
  double taxi_final_angle;
  double last_event_time;
  double taxi_rotate_speed;
  double spawn_x;
  double spawn_z;
  CFVector3 current_taxi_pos;
  CFVector3 current_taxi_our_pos;
};

int Fail(const char* message) {
  std::cerr << "legacy-vehicle-state-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool WriteFixture(const char* path, VehicleStaticFixture& fixture) {
  PIN_SaveFile output;
  if (!output.OpenWrite(const_cast<char*>(path))) {
    return false;
  }

  const bool written =
      output.WriteData(reinterpret_cast<char*>(&fixture.dead), sizeof(bool)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.current_time),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.taking_taxi),
                       sizeof(int)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.taxi_current_angle),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.taxi_final_angle),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.last_event_time),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.taxi_rotate_speed),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.spawn_x),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.spawn_z),
                       sizeof(double)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.current_taxi_pos),
                       sizeof(CFVector3)) &&
      output.WriteData(reinterpret_cast<char*>(&fixture.current_taxi_our_pos),
                       sizeof(CFVector3));
  output.Close();
  return written;
}

bool ReadFixture(const char* path, VehicleStaticFixture& fixture) {
  PIN_SaveFile input;
  if (!input.OpenRead(const_cast<char*>(path))) {
    return false;
  }

  const bool read =
      input.GetData(reinterpret_cast<char*>(&fixture.dead), sizeof(bool)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.current_time),
                    sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.taking_taxi), sizeof(int)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.taxi_current_angle),
                    sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.taxi_final_angle),
                    sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.last_event_time),
                    sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.taxi_rotate_speed),
                    sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.spawn_x), sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.spawn_z), sizeof(double)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.current_taxi_pos),
                    sizeof(CFVector3)) &&
      input.GetData(reinterpret_cast<char*>(&fixture.current_taxi_our_pos),
                    sizeof(CFVector3));
  const bool at_end = input.GetCurrentData() == nullptr;
  input.Close();
  return read && at_end;
}

bool SameVector(const CFVector3& left, const CFVector3& right) {
  return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool SameFixture(const VehicleStaticFixture& left,
                 const VehicleStaticFixture& right) {
  return left.dead == right.dead &&
         left.current_time == right.current_time &&
         left.taking_taxi == right.taking_taxi &&
         left.taxi_current_angle == right.taxi_current_angle &&
         left.taxi_final_angle == right.taxi_final_angle &&
         left.last_event_time == right.last_event_time &&
         left.taxi_rotate_speed == right.taxi_rotate_speed &&
         left.spawn_x == right.spawn_x && left.spawn_z == right.spawn_z &&
         SameVector(left.current_taxi_pos, right.current_taxi_pos) &&
         SameVector(left.current_taxi_our_pos, right.current_taxi_our_pos);
}

bool FilesEqual(const char* left_path, const char* right_path) {
  std::ifstream left(left_path, std::ios::binary | std::ios::ate);
  std::ifstream right(right_path, std::ios::binary | std::ios::ate);
  if (!left || !right || left.tellg() != right.tellg()) {
    return false;
  }

  left.seekg(0);
  right.seekg(0);
  return std::equal(std::istreambuf_iterator<char>(left),
                    std::istreambuf_iterator<char>(),
                    std::istreambuf_iterator<char>(right));
}

bool ExerciseStateRoundTrip(const char* input_path, const char* output_path) {
  VehicleStaticFixture source = {
      true,
      12.5,
      1,
      -0.75,
      2.25,
      125.125,
      6.5,
      -1024.25,
      2048.75,
      CFVector3(1.5, -2.0, 3.25),
      CFVector3(-4.5, 5.75, -6.125),
  };
  if (!WriteFixture(input_path, source)) {
    return false;
  }

  const double unsaved_spawn_y = -333.5;
  Vehicle::m_spY = unsaved_spawn_y;
  PIN_SaveFile input;
  if (!input.OpenRead(const_cast<char*>(input_path)) ||
      !Vehicle::LoadStaticData(input) || input.GetCurrentData() != nullptr ||
      Vehicle::m_spY != unsaved_spawn_y) {
    input.Close();
    return false;
  }
  input.Close();

  PIN_SaveFile output;
  if (!output.OpenWrite(const_cast<char*>(output_path)) ||
      !Vehicle::SaveStaticData(output)) {
    output.Close();
    return false;
  }
  output.Close();

  VehicleStaticFixture restored = {};
  return FilesEqual(input_path, output_path) &&
         ReadFixture(output_path, restored) && SameFixture(source, restored);
}

bool ExerciseDeathCameraState() {
  const double haze_minimum = 200.0;
  const double haze_maximum = 228.0;
  double last_moment = 10.0;
  CFVector3 offset(-12.0, -1.5, 34.0);

  const double preserved_moment = last_moment;
  const CFVector3 preserved_offset = offset;
  if (VehicleDeathCameraState_Advance(
          std::numeric_limits<double>::quiet_NaN(), haze_minimum,
          haze_maximum, 8.0, 0.05, &last_moment, &offset) !=
          RECOVERED_VEHICLE_DEATH_CAMERA_INVALID ||
      last_moment != preserved_moment ||
      !SameVector(offset, preserved_offset)) {
    return false;
  }

  if (VehicleDeathCameraState_Advance(
          20.0, haze_minimum, haze_maximum, 8.0, 0.05,
          &last_moment, &offset) !=
          RECOVERED_VEHICLE_DEATH_CAMERA_ASCENDING ||
      last_moment != 20.0 || offset.x != -12.0 || offset.z != 34.0 ||
      std::fabs(offset.y + 1.9) > 1.0e-12) {
    return false;
  }

  const CFVector3 forward_only = offset;
  if (VehicleDeathCameraState_Advance(
          19.0, haze_minimum, haze_maximum, 8.0, 0.05,
          &last_moment, &offset) !=
          RECOVERED_VEHICLE_DEATH_CAMERA_INVALID ||
      last_moment != 20.0 || !SameVector(offset, forward_only)) {
    return false;
  }

  offset.y = -427.9;
  if (VehicleDeathCameraState_Advance(
          20.05, haze_minimum, haze_maximum, 8.0, 0.05,
          &last_moment, &offset) !=
          RECOVERED_VEHICLE_DEATH_CAMERA_COMPLETE ||
      last_moment != 20.05 || offset.y != -428.0 ||
      !VehicleDeathCameraState_IsComplete(
          offset.y, haze_minimum, haze_maximum)) {
    return false;
  }

  if (VehicleDeathCameraState_Advance(
          200.0, haze_minimum, haze_maximum, 8.0, 0.05,
          &last_moment, &offset) !=
          RECOVERED_VEHICLE_DEATH_CAMERA_COMPLETE ||
      last_moment != 200.0 || offset.y != -428.0) {
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4, "vehicle state requires Win32");
  static_assert(sizeof(bool) == 1, "vehicle save format requires 1-byte bool");
  static_assert(sizeof(int) == 4, "vehicle save format requires 4-byte int");
  static_assert(sizeof(double) == 8,
                "vehicle save format requires 8-byte double");
  static_assert(sizeof(CFVector3) == 24,
                "vehicle vector save layout changed");
  if (argc != 2) {
    return Fail("expected a temporary save path");
  }

  const std::string output_path = std::string(argv[1]) + ".roundtrip";
  std::remove(argv[1]);
  std::remove(output_path.c_str());
  if (!ExerciseStateRoundTrip(argv[1], output_path.c_str())) {
    return Fail("static save round-trip diverged");
  }
  if (!ExerciseDeathCameraState()) {
    return Fail("bounded death-camera state diverged");
  }

  std::remove(argv[1]);
  std::remove(output_path.c_str());
  std::cout << "legacy-vehicle-state-smoke: OK\n";
  return EXIT_SUCCESS;
}
