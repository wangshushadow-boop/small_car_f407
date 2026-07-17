#ifndef SMALL_CAR_HOST_PROTOCOL_HPP_
#define SMALL_CAR_HOST_PROTOCOL_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "small_car_host/types.hpp"

namespace small_car {

constexpr std::uint8_t kProtocolVersion = 0x01;
constexpr std::uint8_t kSync0 = 0xAA;
constexpr std::uint8_t kSync1 = 0x55;
constexpr std::size_t kMaxPayloadSize = 64;

enum class Msg : std::uint8_t {
  kControl = 0x01,
  kServo = 0x02,
  kHeartbeat = 0x03,
  kParam = 0x04,
  kChassisStatus = 0x81,
  kEncoderDelta = 0x82,
  kImuRaw = 0x83,
  kDeviceStatus = 0x84,
  kAck = 0x85,
  kOdometry = 0x86,
};

struct Frame {
  std::uint8_t msg = 0;
  std::uint8_t seq = 0;
  std::vector<std::uint8_t> payload;
};

using DecodedMessage =
    std::variant<ChassisStatus, EncoderDelta, ImuRaw, DeviceStatus, Ack, Odometry>;

std::uint16_t Crc16CcittFalse(const std::uint8_t* data, std::size_t size);
std::uint16_t Crc16CcittFalse(const std::vector<std::uint8_t>& data);

std::uint32_t NowMs();

std::vector<std::uint8_t> EncodeFrame(std::uint8_t msg,
                                      std::uint8_t seq,
                                      const std::vector<std::uint8_t>& payload);

std::vector<std::uint8_t> MakeHeartbeatFrame(std::uint8_t seq,
                                             std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeStopFrame(std::uint8_t seq,
                                        std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeDriveFrame(std::uint8_t seq,
                                         std::int16_t forward,
                                         std::int16_t turn,
                                         std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeServoFrame(std::uint8_t seq,
                                         std::uint16_t left_us,
                                         std::uint16_t right_us,
                                         std::uint32_t host_time_ms = NowMs());

std::optional<DecodedMessage> DecodePayload(const Frame& frame);

class FrameParser {
 public:
  std::vector<Frame> Feed(const std::uint8_t* data, std::size_t size);
  std::vector<Frame> Feed(const std::vector<std::uint8_t>& data);
  void Reset();

 private:
  std::vector<std::uint8_t> buffer_;
};

class PacketCodec {
 public:
  std::vector<std::uint8_t> Heartbeat();
  std::vector<std::uint8_t> Stop();
  std::vector<std::uint8_t> Drive(std::int16_t forward, std::int16_t turn);
  std::vector<std::uint8_t> Servo(std::uint16_t left_us, std::uint16_t right_us);

 private:
  std::uint8_t NextSeq();

  std::uint8_t seq_ = 0;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_PROTOCOL_HPP_
