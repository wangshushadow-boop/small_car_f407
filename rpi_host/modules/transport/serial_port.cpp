#include "small_car_host/serial_port.hpp"

/*
 * Linux 串口封装模块。
 *
 * 这里只处理设备打开、波特率、8N1、raw 模式和非阻塞读写。
 * 上层协议不要直接依赖 termios 细节，统一通过 SerialPort 读写字节。
 */

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace small_car {
namespace {

speed_t ToTermiosBaud(int baudrate) {
  // 当前项目主要使用 115200，这里保留几个常见波特率便于临时调试。
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

  // O_NONBLOCK 让上层 Poll() 可以周期调用，不会因为串口暂时没数据卡住。
  fd_ = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    return false;
  }

  termios options {};
  if (tcgetattr(fd_, &options) != 0) {
    Close();
    return false;
  }

  // raw 模式会关闭行缓冲、回显和特殊字符处理，适合二进制协议。
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

  // VMIN=0、VTIME=0 配合非阻塞 fd：有多少读多少，没有数据立即返回。
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 0;

  if (tcsetattr(fd_, TCSANOW, &options) != 0) {
    Close();
    return false;
  }

  // 清掉打开串口前残留的数据，避免旧包影响协议解析。
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
      // 非阻塞写入可能短暂不可写，重试即可。
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace small_car
