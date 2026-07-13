#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <vector>

#include "small_car_host/protocol.hpp"

namespace {

void Expect(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

std::vector<std::uint8_t> PayloadChassis() {
  return {
      0x64, 0x00, 0x00, 0x00,  // t=100
      0x02,                    // source=PAD
      0x01,                    // enabled=true
      0x2C, 0x01,              // forward=300
      0xD8, 0xFF,              // turn=-40
      0x30, 0x02,              // ultra=560
  };
}

void TestCrc() {
  const char* text = "123456789";
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(text);
  Expect(small_car::Crc16CcittFalse(bytes, 9) == 0x29B1, "crc known value failed");
}

void TestEncodeParse() {
  const auto raw = small_car::EncodeFrame(
      static_cast<std::uint8_t>(small_car::Msg::kHeartbeat), 7, {0xD2, 0x04, 0x00, 0x00});
  small_car::FrameParser parser;
  const auto frames = parser.Feed(raw);
  Expect(frames.size() == 1, "frame count mismatch");
  Expect(frames[0].seq == 7, "seq mismatch");
  Expect(frames[0].msg == static_cast<std::uint8_t>(small_car::Msg::kHeartbeat),
         "msg mismatch");
  Expect(frames[0].payload.size() == 4, "payload size mismatch");
}

void TestNoiseAndSplit() {
  const auto raw = small_car::EncodeFrame(static_cast<std::uint8_t>(small_car::Msg::kAck),
                                          1,
                                          {0x01, 0x09, 0x00});
  small_car::FrameParser parser;
  Expect(parser.Feed(std::vector<std::uint8_t>{0x00, 0x11, 0xAA}).empty(),
         "noise should not produce frame");

  std::vector<std::uint8_t> part1(raw.begin() + 1, raw.begin() + 4);
  Expect(parser.Feed(part1).empty(), "partial frame should not produce frame");

  std::vector<std::uint8_t> part2(raw.begin() + 4, raw.end());
  const auto frames = parser.Feed(part2);
  Expect(frames.size() == 1, "split frame parse failed");

  const auto decoded = small_car::DecodePayload(frames[0]);
  Expect(decoded.has_value(), "ack decode failed");
  const auto* ack = std::get_if<small_car::Ack>(&decoded.value());
  Expect(ack != nullptr, "ack type mismatch");
  Expect(ack->ack_msg == 0x01 && ack->ack_seq == 0x09 && ack->result == 0x00,
         "ack field mismatch");
}

void TestBadCrcDropped() {
  auto raw = small_car::MakeHeartbeatFrame(2, 1);
  raw.back() ^= 0x01;
  small_car::FrameParser parser;
  Expect(parser.Feed(raw).empty(), "bad crc should be dropped");
}

void TestDecodeChassis() {
  const auto raw = small_car::EncodeFrame(
      static_cast<std::uint8_t>(small_car::Msg::kChassisStatus), 3, PayloadChassis());
  small_car::FrameParser parser;
  const auto frames = parser.Feed(raw);
  const auto decoded = small_car::DecodePayload(frames[0]);
  const auto* status = std::get_if<small_car::ChassisStatus>(&decoded.value());
  Expect(status != nullptr, "chassis type mismatch");
  Expect(status->mcu_time_ms == 100, "time mismatch");
  Expect(status->source == 2, "source mismatch");
  Expect(status->enabled, "enabled mismatch");
  Expect(status->forward == 300, "forward mismatch");
  Expect(status->turn == -40, "turn mismatch");
  Expect(status->ultra_mm == 560, "ultra mismatch");
}

void TestDriveClamp() {
  const auto raw = small_car::MakeDriveFrame(1, 2000, -2000, 1);
  small_car::FrameParser parser;
  const auto frames = parser.Feed(raw);
  Expect(frames.size() == 1, "drive frame parse failed");
  const auto& payload = frames[0].payload;
  Expect(payload[6] == 0xE8 && payload[7] == 0x03, "forward clamp mismatch");
  Expect(payload[8] == 0x18 && payload[9] == 0xFC, "turn clamp mismatch");
}

}  // namespace

int main() {
  try {
    TestCrc();
    TestEncodeParse();
    TestNoiseAndSplit();
    TestBadCrcDropped();
    TestDecodeChassis();
    TestDriveClamp();
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }

  std::cout << "protocol_test passed\n";
  return 0;
}
