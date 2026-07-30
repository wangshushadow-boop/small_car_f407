#ifndef BOOTLOADER_SHA256_H
#define BOOTLOADER_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t buffer[64];
} Sha256Context;

void Sha256_Init(Sha256Context *context);
void Sha256_Update(Sha256Context *context, const uint8_t *data, size_t length);
void Sha256_Final(Sha256Context *context, uint8_t digest[32]);

#endif
