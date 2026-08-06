/**
 * @file audio_device.hpp
 * @brief 声明基于 ALSA 的录音、播放和 WAV 保存接口。
 *
 * 本模块直接调用 ALSA C API，不启动外部 arecord/aplay 进程。当前实现统一
 * 使用 S16_LE 交错 PCM 数据，便于语音算法直接复用录音结果。
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace small_car {

/** 一个有符号 16 位 PCM 采样点。多声道数据按帧交错排列。 */
using PcmSample = int16_t;

/** 音频设备和采样参数。 */
struct AudioConfig {
  /** ALSA PCM 设备名；plughw 可自动完成部分格式转换。 */
  std::string device = "plughw:0,0";
  /** 每秒采样数。 */
  uint32_t sample_rate = 16000;
  /** 声道数。 */
  uint32_t channels = 1;
  /** ALSA 每次读写的目标帧数。 */
  uint32_t period_frames = 1024;
};

/** 同步音频封装；每次操作内部独立打开并关闭 ALSA PCM 句柄。 */
class AudioDevice {
 public:
  /** 保存配置，不会在构造阶段访问硬件。 */
  explicit AudioDevice(AudioConfig config);

  /** 从麦克风录制指定秒数，返回交错排列的 S16_LE PCM 数据。 */
  std::vector<PcmSample> RecordSeconds(uint32_t seconds) const;

  /** 按当前配置播放 RecordSeconds() 返回的 PCM 数据。 */
  void Play(const std::vector<PcmSample>& samples) const;

  /** 将 PCM 数据保存为带标准文件头的 WAV 文件。 */
  void SaveWav(const std::string& path,
               const std::vector<PcmSample>& samples) const;

 private:
  /** 构造时固定的设备参数。 */
  AudioConfig config_;
};

}  // namespace small_car
