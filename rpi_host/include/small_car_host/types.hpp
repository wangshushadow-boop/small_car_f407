#ifndef SMALL_CAR_HOST_TYPES_HPP_
#define SMALL_CAR_HOST_TYPES_HPP_

#include <cstdint>

namespace small_car {

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

struct Ack {
  std::uint8_t ack_msg = 0;
  std::uint8_t ack_seq = 0;
  std::uint8_t result = 0;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_TYPES_HPP_
