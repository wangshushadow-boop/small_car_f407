/**
 * @file audio_melody.cpp
 * @brief 生成一段简单 PCM 旋律并保存为 WAV 文件。
 *
 * 该工具不访问音频硬件，主要用于准备可交给 audio_loopback 播放的测试音频。
 */
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

/** 按 WAV 规定的小端序写入 16 位无符号整数。 */
static void WriteU16(std::ofstream &file, uint16_t value) {
  file.put(static_cast<char>(value & 0xff));
  file.put(static_cast<char>((value >> 8) & 0xff));
}

/** 按 WAV 规定的小端序写入 32 位无符号整数。 */
static void WriteU32(std::ofstream &file, uint32_t value) {
  file.put(static_cast<char>(value & 0xff));
  file.put(static_cast<char>((value >> 8) & 0xff));
  file.put(static_cast<char>((value >> 16) & 0xff));
  file.put(static_cast<char>((value >> 24) & 0xff));
}

int main() {
  // 使用 48 kHz 双声道，和当前 USB 音响的原生播放参数一致。
  const int sample_rate = 48000;
  const int channels = 2;
  // 频率单位为 Hz，每个音符持续 0.3 秒。
  const double notes[] = {523.25, 587.33, 659.25, 783.99, 659.25, 587.33, 523.25, 392.00,
                          440.00, 493.88, 523.25, 587.33, 523.25, 493.88, 440.00, 392.00};
  const int samples_per_note = sample_rate * 3 / 10;
  std::vector<int16_t> pcm;
  pcm.reserve((sizeof(notes) / sizeof(notes[0])) * samples_per_note * channels);

  // 每个单声道采样复制到左右声道，生成交错排列的立体声 PCM。
  for (double freq : notes) {
    for (int i = 0; i < samples_per_note; ++i) {
      const double t = static_cast<double>(i) / sample_rate;
      double env = 1.0;
      // 简单淡入淡出，避免每个音符边界出现明显爆音。
      if (i < 1200) {
        env = static_cast<double>(i) / 1200.0;
      }
      if (i > samples_per_note - 2400) {
        env = static_cast<double>(samples_per_note - i) / 2400.0;
      }
      const int16_t sample =
          static_cast<int16_t>(std::sin(2.0 * M_PI * freq * t) * env * 6000.0);
      pcm.push_back(sample);
      pcm.push_back(sample);
    }
  }

  std::ofstream file("/tmp/codex_melody.wav", std::ios::binary);
  const uint32_t data_bytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
  // 写入标准 WAV 头，生成后可直接用 aplay 播放。
  file.write("RIFF", 4);
  WriteU32(file, 36 + data_bytes);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  WriteU32(file, 16);
  WriteU16(file, 1);
  WriteU16(file, channels);
  WriteU32(file, sample_rate);
  WriteU32(file, sample_rate * channels * 2);
  WriteU16(file, channels * 2);
  WriteU16(file, 16);
  file.write("data", 4);
  WriteU32(file, data_bytes);
  file.write(reinterpret_cast<const char *>(pcm.data()), data_bytes);
  return file.good() ? 0 : 1;
}
