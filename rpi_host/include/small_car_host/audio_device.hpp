#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace small_car {

// 一帧 16 bit PCM 采样，当前模块按单声道录音、单声道播放处理。
using PcmSample = int16_t;

struct AudioConfig {
  // ALSA 设备名。plughw 可以自动做部分格式转换，调试阶段更省心。
  std::string device = "plughw:0,0";
  uint32_t sample_rate = 16000;
  uint32_t channels = 1;
  uint32_t period_frames = 1024;
};

class AudioDevice {
 public:
  explicit AudioDevice(AudioConfig config);

  // 从麦克风录制指定时长，返回交错排列的 S16_LE PCM 数据。
  std::vector<PcmSample> RecordSeconds(uint32_t seconds) const;

  // 播放 RecordSeconds 返回的 PCM 数据。
  void Play(const std::vector<PcmSample>& samples) const;

  // 保存为标准 WAV 文件，便于用其它工具检查录音是否正常。
  void SaveWav(const std::string& path,
               const std::vector<PcmSample>& samples) const;

 private:
  AudioConfig config_;
};

}  // namespace small_car
