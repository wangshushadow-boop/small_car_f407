#include "sha256.h"

#include <string.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32U - (n))))

static const uint32_t k_table[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

static uint32_t ReadBe32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) | ((uint32_t)p[2] << 8U) | p[3];
}

static void Transform(Sha256Context *context, const uint8_t block[64])
{
  uint32_t w[64];
  for (uint32_t i = 0; i < 16U; ++i) w[i] = ReadBe32(&block[i * 4U]);
  for (uint32_t i = 16U; i < 64U; ++i) {
    uint32_t s0 = ROTR(w[i - 15U], 7U) ^ ROTR(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
    uint32_t s1 = ROTR(w[i - 2U], 17U) ^ ROTR(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
    w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
  }
  uint32_t a = context->state[0], b = context->state[1], c = context->state[2];
  uint32_t d = context->state[3], e = context->state[4], f = context->state[5];
  uint32_t g = context->state[6], h = context->state[7];
  for (uint32_t i = 0; i < 64U; ++i) {
    uint32_t s1 = ROTR(e, 6U) ^ ROTR(e, 11U) ^ ROTR(e, 25U);
    uint32_t t1 = h + s1 + ((e & f) ^ ((~e) & g)) + k_table[i] + w[i];
    uint32_t s0 = ROTR(a, 2U) ^ ROTR(a, 13U) ^ ROTR(a, 22U);
    uint32_t t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  context->state[0] += a; context->state[1] += b; context->state[2] += c;
  context->state[3] += d; context->state[4] += e; context->state[5] += f;
  context->state[6] += g; context->state[7] += h;
}

void Sha256_Init(Sha256Context *context)
{
  static const uint32_t initial[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  memcpy(context->state, initial, sizeof(initial));
  context->bit_count = 0U;
}

void Sha256_Update(Sha256Context *context, const uint8_t *data, size_t length)
{
  size_t used = (size_t)((context->bit_count >> 3U) & 63U);
  context->bit_count += (uint64_t)length * 8U;
  while (length != 0U) {
    size_t count = 64U - used;
    if (count > length) count = length;
    memcpy(&context->buffer[used], data, count);
    used += count; data += count; length -= count;
    if (used == 64U) { Transform(context, context->buffer); used = 0U; }
  }
}

void Sha256_Final(Sha256Context *context, uint8_t digest[32])
{
  uint64_t bits = context->bit_count;
  uint8_t pad[72] = {0x80U};
  size_t used = (size_t)((bits >> 3U) & 63U);
  size_t pad_len = (used < 56U) ? (56U - used) : (120U - used);
  Sha256_Update(context, pad, pad_len);
  uint8_t encoded[8];
  for (uint32_t i = 0; i < 8U; ++i) encoded[7U - i] = (uint8_t)(bits >> (i * 8U));
  Sha256_Update(context, encoded, sizeof(encoded));
  for (uint32_t i = 0; i < 8U; ++i) {
    digest[i * 4U] = (uint8_t)(context->state[i] >> 24U);
    digest[i * 4U + 1U] = (uint8_t)(context->state[i] >> 16U);
    digest[i * 4U + 2U] = (uint8_t)(context->state[i] >> 8U);
    digest[i * 4U + 3U] = (uint8_t)context->state[i];
  }
}
