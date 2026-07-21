#ifndef SMALL_CAR_HOST_TYPES_HPP_
#define SMALL_CAR_HOST_TYPES_HPP_

#include <cstdint>

namespace small_car {

// 下面这些结构体是 MCU 协议解码后的“业务数据”，字段单位尽量直接写在命名中。

enum class ControlSource : std::uint8_t {
  kNone = 0,
  kHost = 1,
  kPad = 2,
  kSafe = 3,
};

enum class ControlMode : std::uint8_t {
  kStop = 0,
  kVelocity = 1,
};

enum class AckResult : std::uint8_t {
  kOk = 0,
  kCrcError = 1,
  kLenError = 2,
  kUnsupported = 3,
  kBusy = 4,
};

struct ChassisStatus {
  std::uint32_t mcu_time_ms = 0;
  std::uint8_t source = 0;
  bool enabled = false;
  std::int16_t forward = 0;
  std::int16_t turn = 0;
  std::int16_t ultra_mm = -1;
};

struct EncoderDelta {
  std::uint32_t mcu_time_ms = 0;
  std::int16_t delta_a = 0;
  std::int16_t delta_b = 0;
  std::int16_t delta_c = 0;
  std::int16_t delta_d = 0;
};

struct ImuRaw {
  std::uint32_t mcu_time_ms = 0;
  std::int16_t ax = 0;
  std::int16_t ay = 0;
  std::int16_t az = 0;
  std::int16_t gx = 0;
  std::int16_t gy = 0;
  std::int16_t gz = 0;
};

struct DeviceStatus {
  std::uint32_t mcu_time_ms = 0;
  bool pad_ok = false;
  bool imu_ok = false;
  bool ultra_ok = false;
  std::uint8_t error = 0;
};

struct Odometry {
  std::uint32_t mcu_time_ms = 0;
  // 位置单位为毫米，角度单位为 mdeg，便于 MCU 端整数传输。
  std::int32_t x_mm = 0;
  std::int32_t y_mm = 0;
  std::int32_t z_mm = 0;
  std::int32_t distance_mm = 0;
  std::int16_t speed_mm_s = 0;
  std::int32_t roll_mdeg = 0;
  std::int32_t pitch_mdeg = 0;
  std::int32_t yaw_mdeg = 0;
  std::int32_t yaw_rate_mdeg_s = 0;
  bool calibrated = false;
  bool wheel_yaw_fused = false;
};

struct OdometryDebug {
  std::uint32_t mcu_time_ms = 0;
  std::int16_t left_speed_mm_s = 0;
  std::int16_t right_speed_mm_s = 0;
  std::int16_t turn_speed_mm_s = 0;
  std::int16_t left_delta_mm = 0;
  std::int16_t right_delta_mm = 0;
};

struct ParamValue {
  std::uint32_t mcu_time_ms = 0;
  std::uint8_t param_id = 0;
  std::int32_t value = 0;
};

struct Ack {
  std::uint8_t ack_msg = 0;
  std::uint8_t ack_seq = 0;
  std::uint8_t result = 0;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_TYPES_HPP_
