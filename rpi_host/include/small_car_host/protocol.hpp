#ifndef SMALL_CAR_HOST_PROTOCOL_HPP_
#define SMALL_CAR_HOST_PROTOCOL_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "small_car_host/types.hpp"

namespace small_car {

// 串口协议固定字段。协议版本和帧头必须和 MCU 侧 raspi_link.c 保持一致。
constexpr std::uint8_t kProtocolVersion = 0x02;
constexpr std::uint8_t kSync0 = 0xAA;
constexpr std::uint8_t kSync1 = 0x55;
constexpr std::size_t kMaxPayloadSize = 64;

// 消息类型分为两段：0x01-0x7F 为树莓派下发，0x80 以上为 MCU 上传。
enum class Msg : std::uint8_t {
  kControl = 0x01,
  kServo = 0x02,
  kHeartbeat = 0x03,
  kParam = 0x04,
  kTelemetryConfig = 0x05,
  kChassisStatus = 0x81,
  kEncoderDelta = 0x82,
  kImuRaw = 0x83,
  kDeviceStatus = 0x84,
  kAck = 0x85,
  kOdometry = 0x86,
  kOdometryDebug = 0x87,
  kParamValue = 0x88,
};

enum Telemetry : std::uint16_t {
  kTelemetryChassis = 1U << 0,
  kTelemetryEncoder = 1U << 1,
  kTelemetryImu = 1U << 2,
  kTelemetryDevice = 1U << 3,
  kTelemetryOdometry = 1U << 4,
  kTelemetryOdometryDebug = 1U << 5,
};

struct Frame {
  // 原始消息类型，保留为 uint8_t 方便处理未知消息。
  std::uint8_t msg = 0;
  // 帧序号由发送方递增，用于 ACK 和后续排查丢帧。
  std::uint8_t seq = 0;
  // 不包含帧头、版本、消息类型、序号、长度和 CRC 的负载数据。
  std::vector<std::uint8_t> payload;
};

using DecodedMessage =
    std::variant<ChassisStatus, EncoderDelta, ImuRaw, DeviceStatus, Ack, Odometry,
                 OdometryDebug, ParamValue>;

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
                                         std::int16_t linear_mm_s,
                                         std::int16_t angular_mrad_s,
                                         std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeServoFrame(std::uint8_t seq,
                                         std::uint16_t left_us,
                                         std::uint16_t right_us,
                                         std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeOdomResetFrame(std::uint8_t seq,
                                             std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeParamSetFrame(std::uint8_t seq,
                                            std::uint8_t param_id,
                                            std::int32_t value,
                                            std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeParamGetFrame(std::uint8_t seq,
                                            std::uint8_t param_id,
                                            std::uint32_t host_time_ms = NowMs());
std::vector<std::uint8_t> MakeTelemetryConfigFrame(std::uint8_t seq,
                                                   std::uint16_t mask);

std::optional<DecodedMessage> DecodePayload(const Frame& frame);

class FrameParser {
 public:
  // Feed 可以多次喂入任意长度字节流，内部会处理半包、粘包和噪声。
  std::vector<Frame> Feed(const std::uint8_t* data, std::size_t size);
  std::vector<Frame> Feed(const std::vector<std::uint8_t>& data);
  void Reset();

 private:
  std::vector<std::uint8_t> buffer_;
};

class PacketCodec {
 public:
  // PacketCodec 负责自动递增 seq，调用方只需要关心具体业务命令。
  std::vector<std::uint8_t> Heartbeat();
  std::vector<std::uint8_t> Stop();
  std::vector<std::uint8_t> Drive(std::int16_t linear_mm_s,
                                  std::int16_t angular_mrad_s);
  std::vector<std::uint8_t> Servo(std::uint16_t left_us, std::uint16_t right_us);
  std::vector<std::uint8_t> OdomReset();
  std::vector<std::uint8_t> ParamSet(std::uint8_t param_id, std::int32_t value);
  std::vector<std::uint8_t> ParamGet(std::uint8_t param_id);
  std::vector<std::uint8_t> TelemetryConfig(std::uint16_t mask);

 private:
  std::uint8_t NextSeq();

  std::uint8_t seq_ = 0;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_PROTOCOL_HPP_
