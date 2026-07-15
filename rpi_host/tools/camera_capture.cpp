#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

int ParseInt(const char* text, int fallback) {
  if (text == nullptr) {
    return fallback;
  }

  char* end = nullptr;
  const long value = std::strtol(text, &end, 10);
  if ((end == text) || (*end != '\0')) {
    return fallback;
  }

  return static_cast<int>(value);
}

}  // namespace

int main(int argc, char* argv[]) {
  const int device = (argc > 1) ? ParseInt(argv[1], 0) : 0;
  const std::string output_path = (argc > 2) ? argv[2] : "camera_capture.jpg";
  const int width = (argc > 3) ? ParseInt(argv[3], 640) : 640;
  const int height = (argc > 4) ? ParseInt(argv[4], 480) : 480;

  cv::VideoCapture camera(device, cv::CAP_V4L2);
  if (!camera.isOpened()) {
    std::cerr << "无法打开摄像头 /dev/video" << device << std::endl;
    return 1;
  }

  // 摄像头驱动可能会返回最接近的分辨率，最终尺寸以下方 frame.cols/rows 为准。
  camera.set(cv::CAP_PROP_FRAME_WIDTH, width);
  camera.set(cv::CAP_PROP_FRAME_HEIGHT, height);

  cv::Mat frame;
  for (int i = 0; i < 10; ++i) {
    // 丢弃前几帧，给自动曝光和白平衡一点稳定时间。
    camera >> frame;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  if (frame.empty()) {
    std::cerr << "获取帧失败" << std::endl;
    return 2;
  }

  if (!cv::imwrite(output_path, frame)) {
    std::cerr << "保存图片失败: " << output_path << std::endl;
    return 3;
  }

  std::cout << "已保存图片: " << output_path << std::endl;
  std::cout << "图片尺寸: " << frame.cols << "x" << frame.rows << std::endl;
  return 0;
}
