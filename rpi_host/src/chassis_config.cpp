#include "small_car_host/chassis_config.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <yaml-cpp/yaml.h>

#include "small_car_host/car_client.hpp"

namespace small_car {
namespace {

struct ParameterDefinition {
  std::uint8_t id;
  const char* name;
  std::int32_t min_value;
  std::int32_t max_value;
};

constexpr std::array<ParameterDefinition, 7> kParameterDefinitions = {{
    {1, "odom_mm_per_tick_num", 1000, 5000},
    {2, "gamepad_forward_start", 0, 1000},
    {3, "gamepad_reverse_start", 0, 1000},
    {4, "gamepad_drive_max", 0, 1000},
    {5, "gamepad_turn_start", 0, 1000},
    {6, "gamepad_turn_max", 0, 1000},
    {7, "ultra_near_distance_mm", 0, 5000},
}};

std::runtime_error ConfigError(const std::string& detail) {
  return std::runtime_error("invalid chassis config: " + detail);
}

}  // namespace

std::string DefaultChassisConfigPath(const char* executable) {
  const auto executable_path = std::filesystem::weakly_canonical(
      std::filesystem::absolute(executable));
  return (executable_path.parent_path().parent_path() / "config" /
          "chassis_params.yaml")
      .string();
}

std::vector<ChassisParameter> LoadChassisConfig(const std::string& path) {
  const YAML::Node root = YAML::LoadFile(path);
  const YAML::Node parameters = root["small_car"]["ros__parameters"];
  if (!parameters || !parameters.IsMap()) {
    throw ConfigError("missing small_car.ros__parameters");
  }

  std::vector<ChassisParameter> result;
  result.reserve(kParameterDefinitions.size());
  for (const auto& definition : kParameterDefinitions) {
    const YAML::Node value_node = parameters[definition.name];
    if (!value_node || !value_node.IsScalar()) {
      throw ConfigError(std::string("missing parameter ") + definition.name);
    }

    std::int64_t value = 0;
    try {
      value = value_node.as<std::int64_t>();
    } catch (const YAML::Exception&) {
      throw ConfigError(std::string("parameter is not an integer: ") + definition.name);
    }
    if (value < definition.min_value || value > definition.max_value) {
      throw ConfigError(std::string("parameter out of range: ") + definition.name);
    }

    result.push_back({definition.id, definition.name, static_cast<std::int32_t>(value)});
  }
  return result;
}

bool ApplyChassisConfig(CarClient* client,
                        const std::vector<ChassisParameter>& parameters,
                        std::chrono::milliseconds timeout,
                        std::string* error) {
  if (client == nullptr || !client->IsOpen()) {
    if (error != nullptr) {
      *error = "serial port is not open";
    }
    return false;
  }

  for (const auto& parameter : parameters) {
    if (!client->SendParamSet(parameter.id, parameter.value) ||
        !client->SendParamGet(parameter.id)) {
      if (error != nullptr) {
        *error = "failed to send parameter " + parameter.name;
      }
      return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool verified = false;
    while (std::chrono::steady_clock::now() < deadline) {
      client->Poll();
      const auto actual = client->GetParamValue();
      if (actual.has_value() && actual->param_id == parameter.id) {
        if (actual->value != parameter.value) {
          if (error != nullptr) {
            std::ostringstream stream;
            stream << "parameter verify failed: " << parameter.name
                   << ", expected=" << parameter.value << ", actual=" << actual->value;
            *error = stream.str();
          }
          return false;
        }
        verified = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!verified) {
      if (error != nullptr) {
        *error = "parameter verify timeout: " + parameter.name;
      }
      return false;
    }
  }
  return true;
}

}  // namespace small_car
