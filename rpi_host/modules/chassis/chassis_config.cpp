#include "small_car_host/chassis_config.hpp"

/*
 * 底盘参数加载与下发模块。
 *
 * YAML 文件是标定参数的唯一记录来源。上位机启动后读取全部参数，
 * 逐项发送给 MCU 并回读校验，确保树莓派和 MCU 使用同一套底盘参数。
 */

#include <array>
#include <chrono>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <yaml-cpp/yaml.h>

#include "small_car_host/car_client.hpp"

namespace small_car {
namespace {

constexpr int kParameterApplyAttempts = 3;
constexpr auto kSerialRetryInterval = std::chrono::milliseconds(5);
constexpr auto kParameterSettleTime = std::chrono::milliseconds(20);

struct ParameterDefinition {
  std::uint8_t id;
  const char* name;
  std::int32_t min_value;
  std::int32_t max_value;
};

constexpr std::array<ParameterDefinition, 23> kParameterDefinitions = {{
    {1, "odom_mm_per_tick_num", 1000, 5000},
    {2, "gamepad_forward_start", 0, 1000},
    {3, "gamepad_reverse_start", 0, 1000},
    {4, "gamepad_drive_max", 0, 1000},
    {5, "gamepad_turn_start", 0, 1000},
    {6, "gamepad_turn_max", 0, 1000},
    {7, "ultra_near_distance_mm", 0, 5000},
    {8, "gyro_lsb_per_dps_x10", 100, 300},
    {9, "wheel_track_mm", 0, 1000},
    {10, "yaw_gyro_weight_permille", 0, 1000},
    {11, "attitude_gyro_weight_permille", 0, 1000},
    {12, "imu_roll_offset_mdeg", -30000, 30000},
    {13, "imu_pitch_offset_mdeg", -30000, 30000},
    {14, "max_linear_speed_mm_s", 100, 3000},
    {15, "max_angular_speed_mrad_s", 100, 10000},
    {16, "wheel_speed_closed_loop_enabled", 0, 1},
    {17, "wheel_speed_kp_x100", 0, 1000},
    {18, "wheel_speed_ki_x100", 0, 1000},
    {19, "wheel_speed_integral_limit", 0, 10000},
    {20, "wheel_accel_limit_mm_s2", 50, 5000},
    {21, "wheel_pwm_min", 0, 1000},
    {22, "wheel_left_output_permille", 500, 1500},
    {23, "wheel_right_output_permille", 500, 1500},
}};

std::runtime_error ConfigError(const std::string& detail) {
  return std::runtime_error("invalid chassis config: " + detail);
}

template <typename Sender>
bool SendWithRetry(CarClient* client, Sender sender,
                   std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (sender()) {
      return true;
    }

    // USB 串口短暂繁忙时继续接收 MCU 上行数据，腾出驱动缓冲区后再重试。
    client->Poll();
    std::this_thread::sleep_for(kSerialRetryInterval);
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

void PollFor(CarClient* client, std::chrono::milliseconds duration) {
  const auto deadline = std::chrono::steady_clock::now() + duration;
  while (std::chrono::steady_clock::now() < deadline) {
    client->Poll();
    std::this_thread::sleep_for(kSerialRetryInterval);
  }
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
    bool verified = false;
    bool send_failed = false;
    std::optional<std::int32_t> last_value;

    for (int attempt = 0; attempt < kParameterApplyAttempts && !verified; ++attempt) {
      send_failed = false;
      if (!SendWithRetry(client,
                         [&]() {
                           return client->SendParamSet(parameter.id, parameter.value);
                         },
                         timeout)) {
        send_failed = true;
        continue;
      }

      // MCU 会先处理 SET 并返回 ACK；稍作等待，避免 SET/GET 连续帧挤满串口缓冲区。
      PollFor(client, kParameterSettleTime);
      if (!SendWithRetry(client,
                         [&]() { return client->SendParamGet(parameter.id); }, timeout)) {
        send_failed = true;
        continue;
      }

      const auto deadline = std::chrono::steady_clock::now() + timeout;
      while (std::chrono::steady_clock::now() < deadline) {
        client->Poll();
        const auto actual = client->GetParamValue();
        if (actual.has_value() && actual->param_id == parameter.id) {
          last_value = actual->value;
          if (actual->value == parameter.value) {
            verified = true;
          }
          break;
        }
        std::this_thread::sleep_for(kSerialRetryInterval);
      }

      if (!verified) {
        // 两次尝试之间继续清空上行数据，避免旧帧干扰下一次参数回读。
        PollFor(client, kParameterSettleTime);
      }
    }

    if (!verified) {
      if (error != nullptr) {
        if (send_failed) {
          *error = "failed to send parameter after retries: " + parameter.name;
        } else if (last_value.has_value()) {
          std::ostringstream stream;
          stream << "parameter verify failed: " << parameter.name
                 << ", expected=" << parameter.value << ", actual=" << *last_value;
          *error = stream.str();
        } else {
          *error = "parameter verify timeout after retries: " + parameter.name;
        }
      }
      return false;
    }
  }
  return true;
}

}  // namespace small_car
