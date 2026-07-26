/**
 * @file sensor_monitor.cpp
 * @brief 提供 MCU 传感器、里程计和底盘状态的命令行监视工具。
 *
 * 程序启动时可自动从 YAML 下发底盘参数，并按命令行选项选择要打印的遥测类型。
 */
#include "small_car_base/chassis/chassis_config.hpp"
#include "small_car_base/mcu/car_client.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

namespace {

/** 命令行解析后的运行选项。每个 show_* 字段对应一个遥测开关位。 */
struct Options {
  /** MCU 串口设备路径。 */
  std::string port = "/dev/ttyACM0";
  /** 底盘参数文件；为空时根据可执行文件位置自动查找。 */
  std::string config;
  int baudrate = 115200;
  /** 同类消息两次打印之间的最小间隔。 */
  int interval_ms = 100;
  /** 参数下发失败时是否直接退出。 */
  bool strict_config = false;
  bool show_imu = false;
  bool show_encoder = false;
  bool show_ultrasonic = false;
  bool show_chassis = false;
  bool show_device = false;
  bool show_odometry = false;
};

/** 打印命令语法和常用示例。 */
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

/**
 * 解析串口、打印周期和遥测选择参数。
 * 未显式选择数据时默认显示 IMU、编码器和超声，兼顾常见调试需求。
 */
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

/** 判断一个消息类型是否已经达到用户指定的打印周期。 */
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

/** 以下 Print* 函数只负责稳定的单行文本格式，便于终端筛选和日志解析。 */
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

  // sensor_monitor 需要独占串口；运行前应停止正在使用同一设备的 ROS 桥接节点。
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

  // 根据用户选择生成 MCU 遥测掩码，避免无关数据占用 115200 波特率带宽。
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

  // mcu_time_ms 用于识别缓存是否更新，主机时钟仅用于限制终端打印频率。
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
