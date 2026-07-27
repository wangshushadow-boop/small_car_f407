/**
 * @file main.cpp
 * @brief 提供小车串口协议的命令行调试客户端。
 *
 * 工具用于手动发送停止、速度和舵机命令，并查看 MCU 上行状态。
 */
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "small_car_base/chassis/chassis_config.hpp"
#include "small_car_base/mcu/car_client.hpp"

namespace {

/** 打印所有支持的命令和参数格式。 */
void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  small_car_host_cli --port /dev/ttyACM0 [--config <path>]\n"
      << "  small_car_host_cli --port /dev/ttyACM0 monitor [--heartbeat-ms 1000]\n"
      << "  small_car_host_cli --port /dev/ttyACM0 heartbeat\n"
      << "  small_car_host_cli --port /dev/ttyACM0 stop\n"
      << "  small_car_host_cli --port /dev/ttyACM0 drive <linear_mm_s> <angular_mrad_s>\n"
      << "  small_car_host_cli --port /dev/ttyACM0 servo <upper_us> <lower_us>\n";
}

/** 返回指定命令行选项后面的值；选项不存在或缺少值时返回空字符串。 */
std::string ArgValue(int argc, char** argv, const std::string& key) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == key) {
      return argv[i + 1];
    }
  }
  return {};
}

/** 查找第一个业务命令的位置，跳过 --port、--config 等全局选项。 */
int FirstCommandIndex(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "monitor" || arg == "heartbeat" || arg == "stop" || arg == "drive" ||
        arg == "servo") {
      return i;
    }
  }
  return -1;
}

/** 未提供业务命令时，检查是否混入无法识别的额外参数。 */
bool HasUnexpectedArgument(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--port" || arg == "--config") {
      ++i;
      continue;
    }
    return true;
  }
  return false;
}

/**
 * 读取完整底盘参数文件并执行“写入后回读”校验。
 * 返回值直接作为进程退出码，0 表示全部参数已经生效。
 */
int ApplyDefaultConfig(int argc, char** argv, small_car::CarClient* client) {
  const std::string config_arg = ArgValue(argc, argv, "--config");
  const std::string config_path =
      config_arg.empty() ? small_car::DefaultChassisConfigPath(argv[0]) : config_arg;
  try {
    const auto parameters = small_car::LoadChassisConfig(config_path);
    std::string error;
    if (!small_car::ApplyChassisConfig(
            client, parameters, std::chrono::milliseconds(800), &error)) {
      std::cerr << "apply config failed: " << error << "\n";
      return 5;
    }
    for (const auto& parameter : parameters) {
      std::cout << "[CONFIG] id=" << static_cast<int>(parameter.id)
                << " name=" << parameter.name << " value=" << parameter.value << "\n";
    }
    std::cout << "[CONFIG] applied and verified: " << config_path << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "load config failed: " << error.what() << "\n";
    return 5;
  }
}

/** 打印客户端当前缓存的最新状态；没有收到过的消息类型会被跳过。 */
void PrintStatus(const small_car::CarClient& client) {
  if (const auto status = client.GetChassisStatus()) {
    std::cout << "[CHASSIS] t=" << status->mcu_time_ms
              << " src=" << static_cast<int>(status->source)
              << " en=" << status->enabled
              << " type=" << static_cast<int>(status->value_type)
              << " f=" << status->forward
              << " turn=" << status->turn
              << " ultra=" << status->ultra_mm << "\n";
  }
  if (const auto imu = client.GetImuRaw()) {
    std::cout << "[IMU] t=" << imu->mcu_time_ms
              << " ax=" << imu->ax
              << " ay=" << imu->ay
              << " az=" << imu->az
              << " gx=" << imu->gx
              << " gy=" << imu->gy
              << " gz=" << imu->gz << "\n";
  }
  if (const auto device = client.GetDeviceStatus()) {
    std::cout << "[DEV] t=" << device->mcu_time_ms
              << " pad=" << device->pad_ok
              << " imu=" << device->imu_ok
              << " ultra=" << device->ultra_ok
              << " err=" << static_cast<int>(device->error) << "\n";
  }
  if (const auto ack = client.GetLastAck()) {
    std::cout << "[ACK] msg=0x" << std::hex << static_cast<int>(ack->ack_msg)
              << " seq=" << std::dec << static_cast<int>(ack->ack_seq)
              << " result=" << static_cast<int>(ack->result) << "\n";
  }
  std::cout << std::flush;
}

}  // namespace

int main(int argc, char** argv) {
  // 先解析全局选项和业务命令，避免打开串口后才发现参数不完整。
  const std::string port = ArgValue(argc, argv, "--port");
  const int command_index = FirstCommandIndex(argc, argv);
  if (port.empty() || (command_index < 0 && HasUnexpectedArgument(argc, argv))) {
    PrintUsage();
    return 1;
  }

  small_car::CarClient client;
  if (!client.Open(port)) {
    std::cerr << "open serial failed: " << port << "\n";
    return 2;
  }

  // 所有命令执行前统一下发版本化配置，保证调试结果与 ROS 节点一致。
  const int config_result = ApplyDefaultConfig(argc, argv, &client);
  if (config_result != 0 || command_index < 0) {
    return config_result;
  }

  // 单次命令发送成功后立即退出；MCU 的 ACK 可在 monitor 模式中观察。
  const std::string command = argv[command_index];
  if (command == "heartbeat") {
    return client.SendHeartbeat() ? 0 : 3;
  }
  if (command == "stop") {
    return client.SendStop() ? 0 : 3;
  }
  if (command == "drive") {
    if (command_index + 2 >= argc) {
      PrintUsage();
      return 1;
    }
    return client.SendDrive(static_cast<std::int16_t>(std::stoi(argv[command_index + 1])),
                            static_cast<std::int16_t>(std::stoi(argv[command_index + 2])))
               ? 0
               : 3;
  }
  if (command == "servo") {
    if (command_index + 2 >= argc) {
      PrintUsage();
      return 1;
    }
    return client.SendServo(static_cast<std::uint16_t>(std::stoi(argv[command_index + 1])),
                            static_cast<std::uint16_t>(std::stoi(argv[command_index + 2])))
               ? 0
               : 3;
  }
  int heartbeat_ms = 0;
  const std::string heartbeat_arg = ArgValue(argc, argv, "--heartbeat-ms");
  if (!heartbeat_arg.empty()) {
    heartbeat_ms = std::stoi(heartbeat_arg);
  }

  // monitor 模式持续轮询串口，并可按指定周期发送心跳维持主机控制权。
  auto last_heartbeat = std::chrono::steady_clock::now();
  auto last_print = std::chrono::steady_clock::now();
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (heartbeat_ms > 0 &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat).count() >=
            heartbeat_ms) {
      client.SendHeartbeat();
      last_heartbeat = now;
    }

    client.Poll();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print).count() >= 500) {
      PrintStatus(client);
      last_print = now;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
