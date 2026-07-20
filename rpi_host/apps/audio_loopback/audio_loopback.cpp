#include "small_car_host/audio_device.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace {

uint32_t ParseU32(const char* text, uint32_t fallback) {
  try {
    return static_cast<uint32_t>(std::stoul(text));
  } catch (...) {
    return fallback;
  }
}

}  // namespace

int main(int argc, char** argv) {
  small_car::AudioConfig config;
  uint32_t seconds = 5;
  std::string output = "test.wav";

  // 参数保持简单：录音秒数、输出文件、ALSA 设备名。
  if (argc >= 2) {
    seconds = ParseU32(argv[1], seconds);
  }
  if (argc >= 3) {
    output = argv[2];
  }
  if (argc >= 4) {
    config.device = argv[3];
  }

  try {
    // 先录音并保存 WAV，再直接播放同一段 PCM，用来验证麦克风和喇叭链路。
    small_car::AudioDevice audio(config);
    std::cout << "[AUDIO] record " << seconds << "s from " << config.device
              << "\n";
    const auto samples = audio.RecordSeconds(seconds);
    audio.SaveWav(output, samples);
    std::cout << "[AUDIO] saved " << output << ", samples=" << samples.size()
              << "\n";

    std::cout << "[AUDIO] playback " << output << "\n";
    audio.Play(samples);
    std::cout << "[AUDIO] done\n";
  } catch (const std::exception& e) {
    std::cerr << "[AUDIO] error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
