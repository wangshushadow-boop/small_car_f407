#include "small_car_host/car_client.hpp"

/*
 * MCU 客户端模块。
 *
 * CarClient 是上位机其它模块访问 MCU 的统一入口：
 * - 对外提供语义化方法，例如 SendDrive、SendServo、SendParamSet。
 * - 内部负责调用协议编码、串口收发和最近一帧状态缓存。
 * - CLI、sensor_monitor 和 ROS2 bridge 都通过它复用同一套通信逻辑。
 */

namespace small_car {

bool CarClient::Open(const std::string& device, int baudrate) {
  return serial_.Open(device, baudrate);
}

void CarClient::Close() {
  serial_.Close();
}

bool CarClient::IsOpen() const {
  return serial_.IsOpen();
}

bool CarClient::SendHeartbeat() {
  return SendBytes(codec_.Heartbeat());
}

bool CarClient::SendStop() {
  return SendBytes(codec_.Stop());
}

bool CarClient::SendDrive(std::int16_t linear_mm_s, std::int16_t angular_mrad_s) {
  return SendBytes(codec_.Drive(linear_mm_s, angular_mrad_s));
}

bool CarClient::SendServo(std::uint16_t left_us, std::uint16_t right_us) {
  return SendBytes(codec_.Servo(left_us, right_us));
}

bool CarClient::SendOdomReset() {
  return SendBytes(codec_.OdomReset());
}

bool CarClient::SendParamSet(std::uint8_t param_id, std::int32_t value) {
  return SendBytes(codec_.ParamSet(param_id, value));
}

bool CarClient::SendParamGet(std::uint8_t param_id) {
  return SendBytes(codec_.ParamGet(param_id));
}

void CarClient::Poll() {
  // 上位机主循环应周期调用 Poll()。这里一次最多读 256 字节，
  // FrameParser 会自动处理半包、粘包和噪声。
  const auto data = serial_.Read(256);
  for (const auto& frame : parser_.Feed(data)) {
    HandleFrame(frame);
  }
}

std::optional<ChassisStatus> CarClient::GetChassisStatus() const {
  return chassis_status_;
}

std::optional<EncoderDelta> CarClient::GetEncoderDelta() const {
  return encoder_delta_;
}

std::optional<ImuRaw> CarClient::GetImuRaw() const {
  return imu_raw_;
}

std::optional<DeviceStatus> CarClient::GetDeviceStatus() const {
  return device_status_;
}

std::optional<Odometry> CarClient::GetOdometry() const {
  return odometry_;
}

std::optional<OdometryDebug> CarClient::GetOdometryDebug() const {
  return odometry_debug_;
}

std::optional<ParamValue> CarClient::GetParamValue() const {
  return param_value_;
}

std::optional<Ack> CarClient::GetLastAck() const {
  return last_ack_;
}

bool CarClient::SendBytes(const std::vector<std::uint8_t>& data) {
  return serial_.Write(data);
}

void CarClient::HandleFrame(const Frame& frame) {
  const auto decoded = DecodePayload(frame);
  if (!decoded.has_value()) {
    return;
  }

  // CarClient 只缓存每类消息的最近一帧。算法层需要历史数据时，
  // 应在自己的模块中读取后再做队列或滤波。
  if (const auto* value = std::get_if<ChassisStatus>(&decoded.value())) {
    chassis_status_ = *value;
  } else if (const auto* value = std::get_if<EncoderDelta>(&decoded.value())) {
    encoder_delta_ = *value;
  } else if (const auto* value = std::get_if<ImuRaw>(&decoded.value())) {
    imu_raw_ = *value;
  } else if (const auto* value = std::get_if<DeviceStatus>(&decoded.value())) {
    device_status_ = *value;
  } else if (const auto* value = std::get_if<Odometry>(&decoded.value())) {
    odometry_ = *value;
  } else if (const auto* value = std::get_if<OdometryDebug>(&decoded.value())) {
    odometry_debug_ = *value;
  } else if (const auto* value = std::get_if<ParamValue>(&decoded.value())) {
    param_value_ = *value;
  } else if (const auto* value = std::get_if<Ack>(&decoded.value())) {
    last_ack_ = *value;
  }
}

}  // namespace small_car
