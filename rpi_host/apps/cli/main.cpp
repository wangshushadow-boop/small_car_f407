#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "small_car_host/car_client.hpp"
#include "small_car_host/chassis_config.hpp"

namespace {

void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  small_car_host_cli --port /dev/ttyACM0 [--config <path>]\n"
      << "  small_car_host_cli --port /dev/ttyACM0 monitor [--heartbeat-ms 1000]\n"
      << "  small_car_host_cli --port /dev/ttyACM0 heartbeat\n"
      << "  small_car_host_cli --port /dev/ttyACM0 stop\n"
      << "  small_car_host_cli --port /dev/ttyACM0 drive <forward> <turn>\n"
      << "  small_car_host_cli --port /dev/ttyACM0 servo <left_us> <right_us>\n"
      << "  small_car_host_cli --port /dev/ttyACM0 odom-reset\n";
}

std::string ArgValue(int argc, char** argv, const std::string& key) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == key) {
      return argv[i + 1];
    }
  }
  return {};
}

int FirstCommandIndex(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "monitor" || arg == "heartbeat" || arg == "stop" || arg == "drive" ||
        arg == "servo" || arg == "odom-reset") {
      return i;
    }
  }
  return -1;
}

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

void PrintStatus(const small_car::CarClient& client) {
  if (const auto status = client.GetChassisStatus()) {
    std::cout << "[CHASSIS] t=" << status->mcu_time_ms
              << " src=" << static_cast<int>(status->source)
              << " en=" << status->enabled
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

  const int config_result = ApplyDefaultConfig(argc, argv, &client);
  if (config_result != 0 || command_index < 0) {
    return config_result;
  }

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
  if (command == "odom-reset") {
    return client.SendOdomReset() ? 0 : 3;
  }

  int heartbeat_ms = 0;
  const std::string heartbeat_arg = ArgValue(argc, argv, "--heartbeat-ms");
  if (!heartbeat_arg.empty()) {
    heartbeat_ms = std::stoi(heartbeat_arg);
  }

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
