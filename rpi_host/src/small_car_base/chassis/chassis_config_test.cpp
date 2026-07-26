#include <iostream>
#include <stdexcept>
#include <string>

#include "small_car_base/chassis/chassis_config.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    throw std::runtime_error("expected chassis YAML path");
  }

  const auto parameters = small_car::LoadChassisConfig(argv[1]);
  Expect(parameters.size() == 23, "chassis parameter count mismatch");
  Expect(small_car::ChassisParameterValue(
             parameters, "max_linear_speed_mm_s") > 0,
         "linear speed limit must be positive");
  Expect(small_car::ChassisParameterValue(
             parameters, "max_angular_speed_mrad_s") > 0,
         "angular speed limit must be positive");
  Expect(small_car::ChassisParameterValue(
             parameters, "ultra_near_distance_mm") >= 0,
         "front stop distance must not be negative");

  bool missing_parameter_rejected = false;
  try {
    (void)small_car::ChassisParameterValue(parameters, "missing");
  } catch (const std::runtime_error&) {
    missing_parameter_rejected = true;
  }
  Expect(missing_parameter_rejected,
         "missing chassis parameter must be rejected");

  std::cout << "chassis config tests passed\n";
  return 0;
}
