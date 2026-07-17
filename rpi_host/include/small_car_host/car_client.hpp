#ifndef SMALL_CAR_HOST_CAR_CLIENT_HPP_
#define SMALL_CAR_HOST_CAR_CLIENT_HPP_

#include <cstdint>
#include <optional>
#include <string>

#include "small_car_host/protocol.hpp"
#include "small_car_host/serial_port.hpp"
#include "small_car_host/types.hpp"

namespace small_car {

class CarClient {
 public:
  bool Open(const std::string& device, int baudrate = 115200);
  void Close();
  bool IsOpen() const;

  bool SendHeartbeat();
  bool SendStop();
  bool SendDrive(std::int16_t forward, std::int16_t turn);
  bool SendServo(std::uint16_t left_us, std::uint16_t right_us);

  void Poll();

  std::optional<ChassisStatus> GetChassisStatus() const;
  std::optional<EncoderDelta> GetEncoderDelta() const;
  std::optional<ImuRaw> GetImuRaw() const;
  std::optional<DeviceStatus> GetDeviceStatus() const;
  std::optional<Odometry> GetOdometry() const;
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
  std::optional<Ack> last_ack_;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_CAR_CLIENT_HPP_
