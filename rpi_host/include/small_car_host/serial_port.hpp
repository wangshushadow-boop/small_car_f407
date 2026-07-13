#ifndef SMALL_CAR_HOST_SERIAL_PORT_HPP_
#define SMALL_CAR_HOST_SERIAL_PORT_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace small_car {

class SerialPort {
 public:
  SerialPort() = default;
  ~SerialPort();

  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  bool Open(const std::string& device, int baudrate);
  void Close();
  bool IsOpen() const;

  std::vector<std::uint8_t> Read(std::size_t max_size);
  bool Write(const std::vector<std::uint8_t>& data);

 private:
  int fd_ = -1;
};

}  // namespace small_car

#endif  // SMALL_CAR_HOST_SERIAL_PORT_HPP_
