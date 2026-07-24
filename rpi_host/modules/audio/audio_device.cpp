/**
 * @file audio_device.cpp
 * @brief 实现基于 ALSA 的同步录音、播放和 WAV 文件写入。
 *
 * ALSA 错误会转换为 C++ 异常，调用者可在应用层统一显示或恢复。
 */
#include "small_car_host/audio_device.hpp"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace small_car {
namespace {

class PcmHandle {
 public:
  PcmHandle(const std::string& device, snd_pcm_stream_t stream) {
    // snd_pcm_t 需要成对 open/close，封装成 RAII 可以避免异常路径泄漏句柄。
    const int ret = snd_pcm_open(&handle_, device.c_str(), stream, 0);
    if (ret < 0) {
      throw std::runtime_error("snd_pcm_open failed: " +
                               std::string(snd_strerror(ret)));
    }
  }

  ~PcmHandle() {
    if (handle_ != nullptr) {
      snd_pcm_close(handle_);
    }
  }

  PcmHandle(const PcmHandle&) = delete;
  PcmHandle& operator=(const PcmHandle&) = delete;

  snd_pcm_t* get() const { return handle_; }

 private:
  snd_pcm_t* handle_ = nullptr;
};

void CheckAlsa(int ret, const char* what) {
  if (ret < 0) {
    throw std::runtime_error(std::string(what) + ": " + snd_strerror(ret));
  }
}

void ConfigurePcm(snd_pcm_t* handle, const AudioConfig& config) {
  snd_pcm_hw_params_t* params = nullptr;
  snd_pcm_hw_params_alloca(&params);

  // 使用交错模式：多声道时数据按 L/R/L/R 排列；当前默认是单声道。
  CheckAlsa(snd_pcm_hw_params_any(handle, params), "snd_pcm_hw_params_any");
  CheckAlsa(snd_pcm_hw_params_set_access(handle, params,
                                         SND_PCM_ACCESS_RW_INTERLEAVED),
            "snd_pcm_hw_params_set_access");
  CheckAlsa(snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE),
            "snd_pcm_hw_params_set_format");
  CheckAlsa(snd_pcm_hw_params_set_channels(handle, params, config.channels),
            "snd_pcm_hw_params_set_channels");

  unsigned int rate = config.sample_rate;
  CheckAlsa(snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr),
            "snd_pcm_hw_params_set_rate_near");
  if (rate != config.sample_rate) {
    // plughw 通常可以做重采样；如果这里失败，说明设备/插件不支持目标采样率。
    throw std::runtime_error("ALSA device does not support requested rate");
  }

  // period_frames 决定每次读写的块大小。太小 CPU 压力高，太大延迟高。
  snd_pcm_uframes_t period = config.period_frames;
  CheckAlsa(snd_pcm_hw_params_set_period_size_near(handle, params, &period,
                                                   nullptr),
            "snd_pcm_hw_params_set_period_size_near");
  CheckAlsa(snd_pcm_hw_params(handle, params), "snd_pcm_hw_params");
  CheckAlsa(snd_pcm_prepare(handle), "snd_pcm_prepare");
}

void RecoverPcm(snd_pcm_t* handle, int ret, const char* what) {
  // 处理 xrun 等临时错误，让录音/播放尽量自动恢复。
  ret = snd_pcm_recover(handle, ret, 1);
  CheckAlsa(ret, what);
}

void WriteU16(std::ofstream& out, uint16_t value) {
  out.put(static_cast<char>(value & 0xFF));
  out.put(static_cast<char>((value >> 8) & 0xFF));
}

void WriteU32(std::ofstream& out, uint32_t value) {
  WriteU16(out, static_cast<uint16_t>(value & 0xFFFF));
  WriteU16(out, static_cast<uint16_t>((value >> 16) & 0xFFFF));
}

void WriteBytes(std::ofstream& out, const char* text, std::size_t size) {
  out.write(text, static_cast<std::streamsize>(size));
}

}  // namespace

AudioDevice::AudioDevice(AudioConfig config) : config_(std::move(config)) {}

std::vector<PcmSample> AudioDevice::RecordSeconds(uint32_t seconds) const {
  PcmHandle pcm(config_.device, SND_PCM_STREAM_CAPTURE);
  ConfigurePcm(pcm.get(), config_);

  // ALSA 的 readi/writei 以“帧”为单位；单声道时 1 帧就是 1 个采样。
  const std::size_t total_frames =
      static_cast<std::size_t>(config_.sample_rate) * seconds;
  const std::size_t total_samples = total_frames * config_.channels;
  std::vector<PcmSample> samples(total_samples);

  std::size_t frames_done = 0;
  while (frames_done < total_frames) {
    const std::size_t frames_left = total_frames - frames_done;
    const snd_pcm_uframes_t frames_now = static_cast<snd_pcm_uframes_t>(
        std::min<std::size_t>(frames_left, config_.period_frames));
    PcmSample* dst = samples.data() + frames_done * config_.channels;
    const int ret = snd_pcm_readi(pcm.get(), dst, frames_now);
    if (ret < 0) {
      // 录音过程中可能出现缓冲区溢出，恢复后继续补齐剩余帧。
      RecoverPcm(pcm.get(), ret, "snd_pcm_readi");
      continue;
    }
    frames_done += static_cast<std::size_t>(ret);
  }

  return samples;
}

void AudioDevice::Play(const std::vector<PcmSample>& samples) const {
  PcmHandle pcm(config_.device, SND_PCM_STREAM_PLAYBACK);
  ConfigurePcm(pcm.get(), config_);

  const std::size_t total_frames = samples.size() / config_.channels;
  std::size_t frames_done = 0;
  while (frames_done < total_frames) {
    const std::size_t frames_left = total_frames - frames_done;
    const snd_pcm_uframes_t frames_now = static_cast<snd_pcm_uframes_t>(
        std::min<std::size_t>(frames_left, config_.period_frames));
    const PcmSample* src = samples.data() + frames_done * config_.channels;
    const int ret = snd_pcm_writei(pcm.get(), src, frames_now);
    if (ret < 0) {
      // 播放过程中可能出现 underrun，恢复后继续发送剩余帧。
      RecoverPcm(pcm.get(), ret, "snd_pcm_writei");
      continue;
    }
    frames_done += static_cast<std::size_t>(ret);
  }

  snd_pcm_drain(pcm.get());
}

void AudioDevice::SaveWav(const std::string& path,
                          const std::vector<PcmSample>& samples) const {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("failed to open wav file: " + path);
  }

  const uint16_t bits_per_sample = 16;
  const uint16_t block_align =
      static_cast<uint16_t>(config_.channels * bits_per_sample / 8);
  const uint32_t byte_rate = config_.sample_rate * block_align;
  const uint32_t data_bytes =
      static_cast<uint32_t>(samples.size() * sizeof(PcmSample));

  // 写入最小 WAV 头：RIFF + fmt + data，方便用 aplay 或播放器直接验证。
  WriteBytes(out, "RIFF", 4);
  WriteU32(out, 36 + data_bytes);
  WriteBytes(out, "WAVE", 4);
  WriteBytes(out, "fmt ", 4);
  WriteU32(out, 16);
  WriteU16(out, 1);
  WriteU16(out, static_cast<uint16_t>(config_.channels));
  WriteU32(out, config_.sample_rate);
  WriteU32(out, byte_rate);
  WriteU16(out, block_align);
  WriteU16(out, bits_per_sample);
  WriteBytes(out, "data", 4);
  WriteU32(out, data_bytes);
  out.write(reinterpret_cast<const char*>(samples.data()),
            static_cast<std::streamsize>(data_bytes));
}

}  // namespace small_car
