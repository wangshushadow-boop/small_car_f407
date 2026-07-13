#include "small_car_host/car_client.hpp"

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

bool CarClient::SendDrive(std::int16_t forward, std::int16_t turn) {
  return SendBytes(codec_.Drive(forward, turn));
}

bool CarClient::SendServo(std::uint16_t left_us, std::uint16_t right_us) {
  return SendBytes(codec_.Servo(left_us, right_us));
}

void CarClient::Poll() {
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

  if (const auto* value = std::get_if<ChassisStatus>(&decoded.value())) {
    chassis_status_ = *value;
  } else if (const auto* value = std::get_if<EncoderDelta>(&decoded.value())) {
    encoder_delta_ = *value;
  } else if (const auto* value = std::get_if<ImuRaw>(&decoded.value())) {
    imu_raw_ = *value;
  } else if (const auto* value = std::get_if<DeviceStatus>(&decoded.value())) {
    device_status_ = *value;
  } else if (const auto* value = std::get_if<Ack>(&decoded.value())) {
    last_ack_ = *value;
  }
}

}  // namespace small_car
