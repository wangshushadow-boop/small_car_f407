#ifndef OTA_BOOT_H
#define OTA_BOOT_H

#include <stdint.h>

#define OTA_BOOT_REQUEST_ADDRESS 0x2001FFF0UL
#define OTA_BOOT_REQUEST_MAGIC 0x4F544131UL
#define OTA_APPLICATION_ADDRESS 0x08020000UL
#define OTA_APPLICATION_LIMIT 0x08080000UL

static inline volatile uint32_t *OtaBoot_RequestWord(void)
{
  return (volatile uint32_t *)OTA_BOOT_REQUEST_ADDRESS;
}

#endif
