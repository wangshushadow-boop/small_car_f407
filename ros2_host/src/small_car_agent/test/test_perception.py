import unittest

from small_car_agent.perception import AgentPerception, AudioFrame, ImageFrame, build_wav
from small_car_agent.voice_activity import EnergyVad


class PerceptionTest(unittest.TestCase):
    def test_builds_model_input_with_jpeg_and_wav(self):
        image = ImageFrame(timestamp_ns=3_000_000_000, jpeg=b"jpeg")
        audio = AudioFrame(
            timestamp_ns=2_980_000_000,
            sample_rate=16000,
            channels=1,
            encoding="pcm_s16le",
            frame_samples=320,
            data=b"\x00\x00" * 320,
        )
        wav = build_wav((audio,))
        perception = AgentPerception(image, (audio,), wav).to_model_input()

        self.assertTrue(perception["image_data_url"].startswith("data:image/jpeg;base64,"))
        self.assertEqual(perception["image_timestamp_ns"], 3_000_000_000)
        self.assertTrue(wav.startswith(b"RIFF"))
        self.assertEqual(perception["audio_duration_ms"], 20.0)

    def test_refuses_mixed_audio_formats(self):
        first = AudioFrame(0, 16000, 1, "pcm_s16le", 320, b"\x00\x00" * 320)
        second = AudioFrame(20_000_000, 48000, 1, "pcm_s16le", 960, b"\x00\x00" * 960)
        self.assertIsNone(build_wav((first, second)))

    def test_vad_emits_segment_after_speech_and_silence(self):
        detector = EnergyVad(energy_threshold=100, min_speech_ms=40, silence_ms=40)

        def frame(timestamp_ns, sample):
            return AudioFrame(timestamp_ns, 16000, 1, "pcm_s16le", 320, sample * 320)

        self.assertIsNone(detector.push(frame(0, b"\x80\x00")))
        self.assertIsNone(detector.push(frame(20_000_000, b"\x80\x00")))
        self.assertIsNone(detector.push(frame(40_000_000, b"\x00\x00")))
        segment = detector.push(frame(60_000_000, b"\x00\x00"))

        self.assertIsNotNone(segment)
        self.assertEqual(len(segment.frames), 4)


if __name__ == "__main__":
    unittest.main()
