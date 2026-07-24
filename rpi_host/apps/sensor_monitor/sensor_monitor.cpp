#include "small_car_host/car_client.hpp"
#include "small_car_host/chassis_config.hpp"

/*
 * 传感器监视工具。
 *
 * 用于在树莓派命令行直接查看 MCU 上传的数据，调试时通常会先停止 ROS2 bridge，
 * 再运行本工具独占串口。默认会尝试下发 YAML 参数，但参数失败时仍继续打印数据。
 */

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

namespace {

struct Options {
  std::string port = "/dev/ttyACM0";
  std::string config;
  int baudrate = 115200;
  int interval_ms = 100;
  bool strict_config = false;
  bool show_imu = false;
  bool show_encoder = false;
  bool show_ultrasonic = false;
  bool show_chassis = false;
  bool show_device = false;
  bool show_odometry = false;
};

void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  sensor_monitor [--port /dev/ttyACM0] [--config <path>] [--all]\n"
      << "  sensor_monitor [--imu] [--enc] [--ultra] [--chassis] [--device] [--odom]\n"
      << "  sensor_monitor [--interval-ms 100] [--strict-config]\n\n"
      << "Examples:\n"
      << "  sensor_monitor --all\n"
      << "  sensor_monitor --imu --enc --ultra\n"
      << "  sensor_monitor --port /dev/ttyACM0 --ultra\n";
}

bool ParseArgs(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return false;
    }
    if (arg == "--port" && i + 1 < argc) {
      options->port = argv[++i];
    } else if (arg == "--config" && i + 1 < argc) {
      options->config = argv[++i];
    } else if (arg == "--baud" && i + 1 < argc) {
      options->baudrate = std::stoi(argv[++i]);
    } else if (arg == "--interval-ms" && i + 1 < argc) {
      options->interval_ms = std::stoi(argv[++i]);
    } else if (arg == "--strict-config") {
      options->strict_config = true;
    } else if (arg == "--imu") {
      options->show_imu = true;
    } else if (arg == "--enc") {
      options->show_encoder = true;
    } else if (arg == "--ultra") {
      options->show_ultrasonic = true;
    } else if (arg == "--chassis") {
      options->show_chassis = true;
    } else if (arg == "--device") {
      options->show_device = true;
    } else if (arg == "--odom") {
      options->show_odometry = true;
    } else if (arg == "--all") {
      options->show_imu = true;
      options->show_encoder = true;
      options->show_ultrasonic = true;
      options->show_chassis = true;
      options->show_device = true;
      options->show_odometry = true;
    } else {
      std::cerr << "unknown argument: " << arg << "\n";
      PrintUsage();
      return false;
    }
  }

  if (!options->show_imu && !options->show_encoder && !options->show_ultrasonic &&
      !options->show_chassis && !options->show_device && !options->show_odometry) {
    options->show_imu = true;
    options->show_encoder = true;
    options->show_ultrasonic = true;
  }

  return true;
}

bool CanPrint(std::chrono::steady_clock::time_point now,
              std::chrono::steady_clock::time_point* last_print,
              int interval_ms) {
  if (interval_ms <= 0) {
    *last_print = now;
    return true;
  }

  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - *last_print).count();
  if (elapsed_ms < interval_ms) {
    return false;
  }

  *last_print = now;
  return true;
}

void PrintImu(const small_car::ImuRaw& imu) {
  std::cout << "[IMU] t=" << imu.mcu_time_ms
            << " ax=" << imu.ax
            << " ay=" << imu.ay
            << " az=" << imu.az
            << " gx=" << imu.gx
            << " gy=" << imu.gy
            << " gz=" << imu.gz << "\n";
}

void PrintEncoder(const small_car::EncoderDelta& enc) {
  std::cout << "[ENC] t=" << enc.mcu_time_ms
            << " A=" << enc.delta_a
            << " B=" << enc.delta_b
            << " C=" << enc.delta_c
            << " D=" << enc.delta_d << "\n";
}

void PrintUltrasonic(const small_car::ChassisStatus& chassis) {
  if (chassis.ultra_mm < 0) {
    std::cout << "[ULTRA] t=" << chassis.mcu_time_ms << " invalid\n";
    return;
  }

  std::cout << "[ULTRA] t=" << chassis.mcu_time_ms
            << " distance=" << chassis.ultra_mm << " mm\n";
}

void PrintChassis(const small_car::ChassisStatus& chassis) {
  std::cout << "[CHASSIS] t=" << chassis.mcu_time_ms
            << " source=" << static_cast<int>(chassis.source)
            << " enabled=" << chassis.enabled
            << " forward=" << chassis.forward
            << " turn=" << chassis.turn
            << " ultra=" << chassis.ultra_mm << "\n";
}

void PrintDevice(const small_car::DeviceStatus& device) {
  std::cout << "[DEVICE] t=" << device.mcu_time_ms
            << " pad=" << device.pad_ok
            << " imu=" << device.imu_ok
            << " ultra=" << device.ultra_ok
            << " error=" << static_cast<int>(device.error) << "\n";
}

void PrintOdometry(const small_car::Odometry& odometry) {
  std::cout << "[ODOM] t=" << odometry.mcu_time_ms
            << " xyz=" << odometry.x_mm << "/" << odometry.y_mm << "/"
            << odometry.z_mm << " mm"
            << " dist=" << odometry.distance_mm << " mm"
            << " speed=" << odometry.speed_mm_s << " mm/s"
            << " rpy=" << (odometry.roll_mdeg / 1000.0) << "/"
            << (odometry.pitch_mdeg / 1000.0) << "/"
            << (odometry.yaw_mdeg / 1000.0) << " deg"
            << " yaw_rate=" << (odometry.yaw_rate_mdeg_s / 1000.0) << " deg/s"
            << " calibrated=" << odometry.calibrated
            << " wheel_fused=" << odometry.wheel_yaw_fused << "\n";
}

void PrintOdometryDebug(const small_car::OdometryDebug& odometry_debug) {
  std::cout << "[ODOM_WHEEL] t=" << odometry_debug.mcu_time_ms
            << " L=" << odometry_debug.left_speed_mm_s << " mm/s"
            << " R=" << odometry_debug.right_speed_mm_s << " mm/s"
            << " turn=" << odometry_debug.turn_speed_mm_s << " mm/s"
            << " dL=" << odometry_debug.left_delta_mm << " mm"
            << " dR=" << odometry_debug.right_delta_mm << " mm\n";
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!ParseArgs(argc, argv, &options)) {
    return 1;
  }

  small_car::CarClient client;
  if (!client.Open(options.port, options.baudrate)) {
    std::cerr << "open serial failed: " << options.port << "\n";
    return 2;
  }

  const std::string config_path = options.config.empty()
                                      ? small_car::DefaultChassisConfigPath(argv[0])
                                      : options.config;
  try {
    const auto parameters = small_car::LoadChassisConfig(config_path);
    std::string error;
    if (!small_car::ApplyChassisConfig(
            &client, parameters, std::chrono::milliseconds(800), &error)) {
      /*
       * sensor_monitor 是现场调试工具，核心目标是尽快看到传感器输出。
       * 如果 MCU 固件暂时没有响应参数协议，直接退出会导致 ENC/ODOM 也看不到。
       * 因此默认只给出警告并继续监视；需要把参数失败当成错误时使用 --strict-config。
       */
      std::cerr << "[CONFIG] warning: apply config failed: " << error << "\n";
      if (options.strict_config) {
        return 3;
      }
    } else {
      std::cout << "[CONFIG] source=" << config_path << "\n";
      for (const auto& parameter : parameters) {
        std::cout << "[CONFIG] id=" << static_cast<int>(parameter.id)
                  << " name=" << parameter.name << " value=" << parameter.value << "\n";
      }
      std::cout << "[CONFIG] applied=" << parameters.size() << " verified=1\n";
    }
  } catch (const std::exception& error) {
    std::cerr << "load config failed: " << error.what() << "\n";
    return 3;
  }

  std::uint16_t telemetry_mask = 0;
  if (options.show_imu) {
    telemetry_mask |= small_car::kTelemetryImu;
  }
  if (options.show_encoder) {
    telemetry_mask |= small_car::kTelemetryEncoder;
  }
  if (options.show_chassis || options.show_ultrasonic) {
    telemetry_mask |= small_car::kTelemetryChassis;
  }
  if (options.show_device) {
    telemetry_mask |= small_car::kTelemetryDevice;
  }
  if (options.show_odometry) {
    telemetry_mask |=
        small_car::kTelemetryOdometry | small_car::kTelemetryOdometryDebug;
  }
  if (!client.SendTelemetryConfig(telemetry_mask)) {
    std::cerr << "[MON] warning: telemetry config send failed\n";
  }

  std::cout << "[MON] port=" << options.port << " baud=" << options.baudrate << "\n";
  std::cout << "[MON] interval=" << options.interval_ms << " ms\n";
  std::cout << "[MON] press Ctrl+C to stop\n";

  std::uint32_t last_imu_time = 0;
  std::uint32_t last_encoder_time = 0;
  std::uint32_t last_chassis_time = 0;
  std::uint32_t last_ultrasonic_time = 0;
  std::uint32_t last_device_time = 0;
  std::uint32_t last_odometry_time = 0;
  std::uint32_t last_odometry_debug_time = 0;
  bool seen_odometry = false;
  bool seen_odometry_debug = false;
  auto last_imu_print = std::chrono::steady_clock::now();
  auto last_encoder_print = last_imu_print;
  auto last_chassis_print = last_imu_print;
  auto last_ultrasonic_print = last_imu_print;
  auto last_device_print = last_imu_print;
  auto last_odometry_print = last_imu_print;
  auto last_odometry_debug_print = last_imu_print;

  while (true) {
    client.Poll();
    const auto now = std::chrono::steady_clock::now();

    if (options.show_imu) {
      if (const auto imu = client.GetImuRaw()) {
        if (imu->mcu_time_ms != last_imu_time &&
            CanPrint(now, &last_imu_print, options.interval_ms)) {
          last_imu_time = imu->mcu_time_ms;
          PrintImu(*imu);
        }
      }
    }

    if (options.show_encoder) {
      if (const auto enc = client.GetEncoderDelta()) {
        if (enc->mcu_time_ms != last_encoder_time &&
            CanPrint(now, &last_encoder_print, options.interval_ms)) {
          last_encoder_time = enc->mcu_time_ms;
          PrintEncoder(*enc);
        }
      }
    }

    if (options.show_chassis || options.show_ultrasonic) {
      if (const auto chassis = client.GetChassisStatus()) {
        if (options.show_chassis && chassis->mcu_time_ms != last_chassis_time &&
            CanPrint(now, &last_chassis_print, options.interval_ms)) {
          last_chassis_time = chassis->mcu_time_ms;
          PrintChassis(*chassis);
        }
        if (options.show_ultrasonic && chassis->mcu_time_ms != last_ultrasonic_time &&
            CanPrint(now, &last_ultrasonic_print, options.interval_ms)) {
          last_ultrasonic_time = chassis->mcu_time_ms;
          PrintUltrasonic(*chassis);
        }
      }
    }

    if (options.show_device) {
      if (const auto device = client.GetDeviceStatus()) {
        if (device->mcu_time_ms != last_device_time &&
            CanPrint(now, &last_device_print, options.interval_ms)) {
          last_device_time = device->mcu_time_ms;
          PrintDevice(*device);
        }
      }
    }

    if (options.show_odometry) {
      if (const auto odometry = client.GetOdometry()) {
        if ((!seen_odometry || odometry->mcu_time_ms != last_odometry_time) &&
            CanPrint(now, &last_odometry_print, options.interval_ms)) {
          seen_odometry = true;
          last_odometry_time = odometry->mcu_time_ms;
          PrintOdometry(*odometry);
        }
      }
      if (const auto odometry_debug = client.GetOdometryDebug()) {
        if ((!seen_odometry_debug ||
             odometry_debug->mcu_time_ms != last_odometry_debug_time) &&
            CanPrint(now, &last_odometry_debug_print, options.interval_ms)) {
          seen_odometry_debug = true;
          last_odometry_debug_time = odometry_debug->mcu_time_ms;
          PrintOdometryDebug(*odometry_debug);
        }
      }
    }

    std::cout << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
