#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Buffer {
  void *start = nullptr;
  size_t length = 0;
};

int Xioctl(int fd, unsigned long request, void *arg) {
  // ioctl 可能被信号打断，遇到 EINTR 时重试原请求。
  int ret = 0;
  do {
    ret = ioctl(fd, request, arg);
  } while ((ret == -1) && (errno == EINTR));
  return ret;
}

int ParseInt(const char *text, int fallback) {
  if (text == nullptr) {
    return fallback;
  }

  char *end = nullptr;
  const long value = std::strtol(text, &end, 10);
  if ((end == text) || (*end != '\0')) {
    return fallback;
  }

  return static_cast<int>(value);
}

uint32_t ParseFormat(const std::string &format) {
  if ((format == "mjpg") || (format == "MJPG") || (format == "jpeg") ||
      (format == "JPEG")) {
    return V4L2_PIX_FMT_MJPEG;
  }

  if ((format == "yuyv") || (format == "YUYV") || (format == "yuv") ||
      (format == "YUV")) {
    return V4L2_PIX_FMT_YUYV;
  }

  return 0;
}

uint8_t ClipRgb(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return static_cast<uint8_t>(value);
}

void YuyvToRgb(const uint8_t *yuyv, int width, int height, std::vector<uint8_t> *rgb) {
  rgb->assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 3U, 0U);

  // YUYV 每 4 字节表示两个像素：Y0 U Y1 V。
  size_t rgb_index = 0;
  const size_t yuyv_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 2U;
  for (size_t i = 0; i + 3 < yuyv_size; i += 4) {
    const int y0 = yuyv[i + 0];
    const int u = yuyv[i + 1] - 128;
    const int y1 = yuyv[i + 2];
    const int v = yuyv[i + 3] - 128;

    const int r0 = y0 + ((1436 * v) >> 10);
    const int g0 = y0 - ((352 * u + 731 * v) >> 10);
    const int b0 = y0 + ((1815 * u) >> 10);
    const int r1 = y1 + ((1436 * v) >> 10);
    const int g1 = y1 - ((352 * u + 731 * v) >> 10);
    const int b1 = y1 + ((1815 * u) >> 10);

    (*rgb)[rgb_index++] = ClipRgb(r0);
    (*rgb)[rgb_index++] = ClipRgb(g0);
    (*rgb)[rgb_index++] = ClipRgb(b0);
    (*rgb)[rgb_index++] = ClipRgb(r1);
    (*rgb)[rgb_index++] = ClipRgb(g1);
    (*rgb)[rgb_index++] = ClipRgb(b1);
  }
}

bool WriteBinaryFile(const std::string &path, const void *data, size_t size) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  file.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
  return file.good();
}

bool WritePpmFile(const std::string &path, int width, int height, const std::vector<uint8_t> &rgb) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  file << "P6\n" << width << " " << height << "\n255\n";
  file.write(reinterpret_cast<const char *>(rgb.data()),
             static_cast<std::streamsize>(rgb.size()));
  return file.good();
}

std::string FourccToString(uint32_t fourcc) {
  char text[5] = {
      static_cast<char>(fourcc & 0xFFU),
      static_cast<char>((fourcc >> 8U) & 0xFFU),
      static_cast<char>((fourcc >> 16U) & 0xFFU),
      static_cast<char>((fourcc >> 24U) & 0xFFU),
      '\0',
  };
  return text;
}

void PrintUsage(const char *program) {
  std::cout << "Usage:\n"
            << "  " << program << " <device> <mjpg|yuyv> <output> [width] [height]\n\n"
            << "Examples:\n"
            << "  " << program << " /dev/video0 mjpg frame.jpg 1920 1080\n"
            << "  " << program << " /dev/video0 yuyv frame.yuyv 1920 1080\n";
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc < 4) {
    PrintUsage(argv[0]);
    return 1;
  }

  const std::string device_path = argv[1];
  const std::string format_text = argv[2];
  const std::string output_path = argv[3];
  const int width = (argc > 4) ? ParseInt(argv[4], 640) : 640;
  const int height = (argc > 5) ? ParseInt(argv[5], 480) : 480;
  const uint32_t pixel_format = ParseFormat(format_text);
  if (pixel_format == 0U) {
    std::cerr << "Unsupported format: " << format_text << std::endl;
    return 1;
  }

  const int fd = open(device_path.c_str(), O_RDWR);
  if (fd < 0) {
    std::perror("open camera");
    return 1;
  }

  v4l2_capability capability {};
  if (Xioctl(fd, VIDIOC_QUERYCAP, &capability) < 0) {
    std::perror("VIDIOC_QUERYCAP");
    close(fd);
    return 1;
  }

  v4l2_format format {};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = static_cast<uint32_t>(width);
  format.fmt.pix.height = static_cast<uint32_t>(height);
  format.fmt.pix.pixelformat = pixel_format;
  format.fmt.pix.field = V4L2_FIELD_NONE;
  if (Xioctl(fd, VIDIOC_S_FMT, &format) < 0) {
    std::perror("VIDIOC_S_FMT");
    close(fd);
    return 1;
  }

  // 驱动可能会调整为最接近的格式，所以后续都使用 actual_*。
  const int actual_width = static_cast<int>(format.fmt.pix.width);
  const int actual_height = static_cast<int>(format.fmt.pix.height);
  const uint32_t actual_format = format.fmt.pix.pixelformat;
  std::cout << "Camera: " << capability.card << std::endl;
  std::cout << "Format: " << FourccToString(actual_format) << " "
            << actual_width << "x" << actual_height << std::endl;

  v4l2_requestbuffers request {};
  request.count = 4;
  request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  request.memory = V4L2_MEMORY_MMAP;
  if (Xioctl(fd, VIDIOC_REQBUFS, &request) < 0) {
    std::perror("VIDIOC_REQBUFS");
    close(fd);
    return 1;
  }

  // 使用 mmap 让驱动直接把图像写入共享缓冲区，避免额外拷贝。
  std::vector<Buffer> buffers(request.count);
  for (uint32_t i = 0; i < request.count; ++i) {
    v4l2_buffer buffer {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;
    if (Xioctl(fd, VIDIOC_QUERYBUF, &buffer) < 0) {
      std::perror("VIDIOC_QUERYBUF");
      close(fd);
      return 1;
    }

    buffers[i].length = buffer.length;
    buffers[i].start = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                            fd, static_cast<off_t>(buffer.m.offset));
    if (buffers[i].start == MAP_FAILED) {
      std::perror("mmap");
      close(fd);
      return 1;
    }
  }

  for (uint32_t i = 0; i < request.count; ++i) {
    v4l2_buffer buffer {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = i;
    if (Xioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
      std::perror("VIDIOC_QBUF");
      close(fd);
      return 1;
    }
  }

  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (Xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
    std::perror("VIDIOC_STREAMON");
    close(fd);
    return 1;
  }

  v4l2_buffer frame {};
  for (int i = 0; i < 8; ++i) {
    // 丢弃前几帧，让曝光和白平衡稳定，再保存最后一帧。
    std::memset(&frame, 0, sizeof(frame));
    frame.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frame.memory = V4L2_MEMORY_MMAP;
    if (Xioctl(fd, VIDIOC_DQBUF, &frame) < 0) {
      std::perror("VIDIOC_DQBUF");
      close(fd);
      return 1;
    }

    if (i < 7) {
      if (Xioctl(fd, VIDIOC_QBUF, &frame) < 0) {
        std::perror("VIDIOC_QBUF");
        close(fd);
        return 1;
      }
    }
  }

  bool ok = false;
  const uint8_t *frame_data = static_cast<const uint8_t *>(buffers[frame.index].start);
  if (actual_format == V4L2_PIX_FMT_MJPEG) {
    ok = WriteBinaryFile(output_path, frame_data, frame.bytesused);
  } else if (actual_format == V4L2_PIX_FMT_YUYV) {
    ok = WriteBinaryFile(output_path, frame_data, frame.bytesused);

    std::vector<uint8_t> rgb;
    YuyvToRgb(frame_data, actual_width, actual_height, &rgb);
    const std::string preview_path = output_path + ".ppm";
    if (WritePpmFile(preview_path, actual_width, actual_height, rgb)) {
      std::cout << "YUYV preview saved: " << preview_path << std::endl;
    }
  }

  if (Xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
    std::perror("VIDIOC_STREAMOFF");
  }

  for (const Buffer &buffer : buffers) {
    if ((buffer.start != nullptr) && (buffer.start != MAP_FAILED)) {
      munmap(buffer.start, buffer.length);
    }
  }
  close(fd);

  if (!ok) {
    std::cerr << "Failed to save output: " << output_path << std::endl;
    return 1;
  }

  std::cout << "Saved: " << output_path << " (" << frame.bytesused << " bytes)"
            << std::endl;
  return 0;
}
