#ifndef SMALL_CAR_HOST_CHASSIS_CONFIG_HPP_
#define SMALL_CAR_HOST_CHASSIS_CONFIG_HPP_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace small_car {

class CarClient;

// YAML 中一项可下发到底盘控制器的参数。
struct ChassisParameter {
  std::uint8_t id = 0;
  std::string name;
  std::int32_t value = 0;
};

// 根据可执行文件位置返回同一工程中的默认参数文件路径。
std::string DefaultChassisConfigPath(const char* executable);

// 读取并校验 ROS2 格式的底盘参数文件。文件缺项或数值越界时抛出异常。
std::vector<ChassisParameter> LoadChassisConfig(const std::string& path);

// 逐项下发参数并回读校验。失败时返回 false，并通过 error 返回原因。
bool ApplyChassisConfig(CarClient* client,
                        const std::vector<ChassisParameter>& parameters,
                        std::chrono::milliseconds timeout,
                        std::string* error);

}  // namespace small_car

#endif  // SMALL_CAR_HOST_CHASSIS_CONFIG_HPP_
