#ifndef OTA_BOOT_H
#define OTA_BOOT_H

#include <stdint.h>

#define OTA_BOOT_REQUEST_ADDRESS 0x2001FFF0UL
#define OTA_BOOT_REQUEST_MAGIC 0x4F544131UL
#define OTA_METADATA_ADDRESS 0x08010000UL
#define OTA_METADATA_MAGIC 0x4F54414DUL
#define OTA_APPLICATION_ADDRESS 0x08020000UL
#define OTA_APPLICATION_LIMIT 0x08080000UL

#define OTA_METADATA_MAGIC_OFFSET 0U
#define OTA_METADATA_SIZE_OFFSET 4U
#define OTA_METADATA_VERSION_OFFSET 8U
#define OTA_METADATA_CRC32_OFFSET 12U

static inline volatile uint32_t *OtaBoot_RequestWord(void)
{
  return (volatile uint32_t *)OTA_BOOT_REQUEST_ADDRESS;
}

static inline uint32_t OtaBoot_ReadMetadataWord(uint32_t offset)
{
  return *(const volatile uint32_t *)(OTA_METADATA_ADDRESS + offset);
}

#endif
