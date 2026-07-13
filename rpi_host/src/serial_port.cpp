#include "small_car_host/serial_port.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace small_car {
namespace {

speed_t ToTermiosBaud(int baudrate) {
  switch (baudrate) {
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
    default:
      return B115200;
  }
}

}  // namespace

SerialPort::~SerialPort() {
  Close();
}

bool SerialPort::Open(const std::string& device, int baudrate) {
  Close();

  fd_ = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    return false;
  }

  termios options {};
  if (tcgetattr(fd_, &options) != 0) {
    Close();
    return false;
  }

  cfmakeraw(&options);
  const speed_t speed = ToTermiosBaud(baudrate);
  cfsetispeed(&options, speed);
  cfsetospeed(&options, speed);

  options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
  options.c_cflag &= static_cast<tcflag_t>(~PARENB);
  options.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
  options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
  options.c_cflag |= CS8;
  options.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);

  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 0;

  if (tcsetattr(fd_, TCSANOW, &options) != 0) {
    Close();
    return false;
  }

  tcflush(fd_, TCIOFLUSH);
  return true;
}

void SerialPort::Close() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

bool SerialPort::IsOpen() const {
  return fd_ >= 0;
}

std::vector<std::uint8_t> SerialPort::Read(std::size_t max_size) {
  std::vector<std::uint8_t> buffer(max_size);
  if (fd_ < 0 || max_size == 0) {
    return {};
  }

  const ssize_t n = read(fd_, buffer.data(), buffer.size());
  if (n > 0) {
    buffer.resize(static_cast<std::size_t>(n));
    return buffer;
  }
  return {};
}

bool SerialPort::Write(const std::vector<std::uint8_t>& data) {
  if (fd_ < 0) {
    return false;
  }

  std::size_t written = 0;
  while (written < data.size()) {
    const ssize_t n = write(fd_, data.data() + written, data.size() - written);
    if (n > 0) {
      written += static_cast<std::size_t>(n);
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace small_car
