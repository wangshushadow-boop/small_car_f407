#ifndef SMALL_CAR_HOST_CAR_CLIENT_HPP_
#define SMALL_CAR_HOST_CAR_CLIENT_HPP_

#include <cstdint>
#include <optional>
#include <string>

#include "small_car_host/protocol.hpp"
#include "small_car_host/serial_port.hpp"
#include "small_car_host/types.hpp"

namespace small_car {

// 上位机访问 MCU 的统一客户端。该类不创建后台线程，调用方需要周期调用 Poll()。
class CarClient {
 public:
  bool Open(const std::string& device, int baudrate = 115200);
  void Close();
  bool IsOpen() const;

  bool SendHeartbeat();
  bool SendStop();
  bool SendDrive(std::int16_t linear_mm_s, std::int16_t angular_mrad_s);
  bool SendServo(std::uint16_t left_us, std::uint16_t right_us);
  bool SendOdomReset();
  bool SendParamSet(std::uint8_t param_id, std::int32_t value);
  bool SendParamGet(std::uint8_t param_id);
  bool SendTelemetryConfig(std::uint16_t mask);

  // 从串口读取可用数据并更新最近一帧缓存；CLI、monitor、ROS2 bridge 都依赖这个入口。
  void Poll();

  // 以下 Get 方法返回最近一次收到的对应消息；没有收到过时返回 std::nullopt。
  std::optional<ChassisStatus> GetChassisStatus() const;
  std::optional<EncoderDelta> GetEncoderDelta() const;
  std::optional<ImuRaw> GetImuRaw() const;
  std::optional<DeviceStatus> GetDeviceStatus() const;
  std::optional<Odometry> GetOdometry() const;
  std::optional<OdometryDebug> GetOdometryDebug() const;
  std::optional<ParamValue> GetParamValue() const;
  std::optional<Ack> GetLastAck() const;

 private:
  bool SendBytes(const std::vector<std::uint8_t>& data);
  void HandleFrame(const Frame& frame);

  SerialPort serial_;
  PacketCodec codec_;
  FrameParser parser_;

  std::optional<ChassisStatus> chassis_status_;
  std::optional<EncoderDelta> encoder_delta_;
  std::optional<ImuRaw> imu_raw_;
  std::optional<DeviceStatus> device_status_;
  std::optional<Odometry> odometry_;
  std::optional<OdometryDebug> odometry_debug_;
  std::optional<ParamValue> param_value_;
  std::optional<Ack> last_ack_;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_CAR_CLIENT_HPP_
