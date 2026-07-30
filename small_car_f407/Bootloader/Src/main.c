#include "stm32f407xx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sha256.h"

#define APP_ADDRESS 0x08020000UL
#define APP_LIMIT 0x08080000UL
#define METADATA_ADDRESS 0x08010000UL
#define BOOT_REQUEST_ADDRESS 0x2001FFF0UL
#define BOOT_REQUEST_MAGIC 0x4F544131UL
#define METADATA_MAGIC 0x4F54414DUL

#define SYNC0 0xAAU
#define SYNC1 0x55U
#define PROTOCOL_VERSION 0x03U
#define MAX_PAYLOAD 64U

#define MSG_OTA_HELLO 0x10U
#define MSG_OTA_BEGIN 0x11U
#define MSG_OTA_DATA 0x12U
#define MSG_OTA_END 0x13U
#define MSG_OTA_BOOT 0x14U
#define MSG_OTA_ABORT 0x15U
#define MSG_ACK 0x85U
#define MSG_OTA_STATUS 0x90U

#define STATUS_OK 0U
#define STATUS_CRC 1U
#define STATUS_LENGTH 2U
#define STATUS_UNSUPPORTED 3U
#define STATUS_STATE 4U
#define STATUS_FLASH 5U
#define STATUS_AUTH 6U
#define STATUS_RANGE 7U

/* Development key only. Replace this value before distributing production firmware. */
static const uint8_t k_hmac_key[32] = {
    0x73, 0x6d, 0x61, 0x6c, 0x6c, 0x2d, 0x63, 0x61, 0x72, 0x2d, 0x6f, 0x74, 0x61, 0x2d, 0x64, 0x65,
    0x76, 0x2d, 0x6b, 0x65, 0x79, 0x2d, 0x76, 0x31, 0x2d, 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x21};

typedef struct {
  uint32_t magic;
  uint32_t image_size;
  uint32_t version;
  uint32_t image_crc32;
  uint8_t image_hmac[32];
} OtaMetadata;

typedef struct {
  uint8_t state;
  uint8_t version;
  uint8_t message;
  uint8_t sequence;
  uint8_t length;
  uint8_t index;
  uint8_t payload[MAX_PAYLOAD];
  uint8_t crc_low;
} Parser;

static Parser g_parser;
static bool g_updating;
static uint32_t g_image_size;
static uint32_t g_image_version;
static uint32_t g_expected_crc;
static uint8_t g_expected_hmac[32];
static uint32_t g_next_offset;
static uint32_t g_running_crc;
static Sha256Context g_hmac_inner;
static Sha256Context g_hmac_outer;

static uint32_t ReadU32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) | ((uint32_t)data[2] << 16U) |
         ((uint32_t)data[3] << 24U);
}

static void WriteU32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
  data[2] = (uint8_t)(value >> 16U);
  data[3] = (uint8_t)(value >> 24U);
}

static uint16_t Crc16(const uint8_t *data, uint32_t length)
{
  uint16_t crc = 0xFFFFU;
  while (length-- != 0U) {
    crc ^= (uint16_t)(*data++) << 8U;
    for (uint32_t bit = 0; bit < 8U; ++bit) crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                                                  : (uint16_t)(crc << 1U);
  }
  return crc;
}

static uint32_t Crc32Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
  while (length-- != 0U) {
    crc ^= *data++;
    for (uint32_t bit = 0; bit < 8U; ++bit) crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return crc;
}

static void HmacInit(Sha256Context *inner, Sha256Context *outer, const uint8_t manifest[12])
{
  uint8_t ipad[64], opad[64];
  memset(ipad, 0x36, sizeof(ipad));
  memset(opad, 0x5c, sizeof(opad));
  for (uint32_t i = 0; i < sizeof(k_hmac_key); ++i) {
    ipad[i] ^= k_hmac_key[i];
    opad[i] ^= k_hmac_key[i];
  }
  Sha256_Init(inner);
  Sha256_Update(inner, ipad, sizeof(ipad));
  Sha256_Update(inner, manifest, 12U);
  Sha256_Init(outer);
  Sha256_Update(outer, opad, sizeof(opad));
}

static void HmacFinal(Sha256Context *inner, Sha256Context *outer, uint8_t digest[32])
{
  uint8_t inner_digest[32];
  Sha256_Final(inner, inner_digest);
  Sha256_Update(outer, inner_digest, sizeof(inner_digest));
  Sha256_Final(outer, digest);
}

static void HardwareInit(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
  (void)RCC->AHB1ENR;

  /* Force all motor direction pins low before accepting update data. */
  GPIOB->MODER = (GPIOB->MODER & ~((3UL << 16U) | (3UL << 18U))) | (1UL << 16U) | (1UL << 18U);
  GPIOE->MODER = (GPIOE->MODER & ~((3UL << 10U) | (3UL << 12U) | (3UL << 18U) | (3UL << 22U) |
                                   (3UL << 26U) | (3UL << 28U))) |
                  (1UL << 10U) | (1UL << 12U) | (1UL << 18U) | (1UL << 22U) | (1UL << 26U) |
                  (1UL << 28U);
  GPIOB->BSRR = (1UL << (8U + 16U)) | (1UL << (9U + 16U));
  GPIOE->BSRR = (1UL << (5U + 16U)) | (1UL << (6U + 16U)) | (1UL << (9U + 16U)) |
                (1UL << (11U + 16U)) | (1UL << (13U + 16U)) | (1UL << (14U + 16U));

  GPIOD->MODER = (GPIOD->MODER & ~((3UL << 16U) | (3UL << 18U))) | (2UL << 16U) | (2UL << 18U);
  GPIOD->AFR[1] = (GPIOD->AFR[1] & ~0xFFUL) | 0x77UL;
  GPIOD->OSPEEDR |= (3UL << 16U) | (3UL << 18U);
  USART3->BRR = 0x008BU; /* 16 MHz HSI, 115200 baud. */
  USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void UartWrite(const uint8_t *data, uint32_t length)
{
  while (length-- != 0U) {
    while ((USART3->SR & USART_SR_TXE) == 0U) {}
    USART3->DR = *data++;
  }
  while ((USART3->SR & USART_SR_TC) == 0U) {}
}

static void SendFrame(uint8_t message, uint8_t sequence, const uint8_t *payload, uint8_t length)
{
  uint8_t frame[6U + MAX_PAYLOAD + 2U] = {SYNC0, SYNC1, PROTOCOL_VERSION, message, sequence, length};
  if (length != 0U) memcpy(&frame[6], payload, length);
  uint16_t crc = Crc16(&frame[2], 4U + length);
  frame[6U + length] = (uint8_t)crc;
  frame[7U + length] = (uint8_t)(crc >> 8U);
  UartWrite(frame, 8U + length);
}

static void SendAck(uint8_t message, uint8_t sequence, uint8_t status)
{
  uint8_t payload[7] = {message, sequence, status, 0, 0, 0, 0};
  WriteU32(&payload[3], g_next_offset);
  SendFrame(MSG_ACK, sequence, payload, sizeof(payload));
}

static bool FlashWait(void)
{
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
  uint32_t errors = FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR;
  bool ok = (FLASH->SR & errors) == 0U;
  FLASH->SR = errors | FLASH_SR_EOP;
  return ok;
}

static void FlashUnlock(void)
{
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
  }
}

static bool FlashEraseSector(uint32_t sector)
{
  if (!FlashWait()) return false;
  FLASH->CR = FLASH_CR_SER | (sector << FLASH_CR_SNB_Pos) | FLASH_CR_PSIZE_1;
  FLASH->CR |= FLASH_CR_STRT;
  bool ok = FlashWait();
  FLASH->CR = 0U;
  return ok;
}

static bool FlashProgram(uint32_t address, const uint8_t *data, uint32_t length)
{
  while (length != 0U) {
    uint32_t word = 0xFFFFFFFFUL;
    uint32_t count = length > 4U ? 4U : length;
    memcpy(&word, data, count);
    FLASH->CR = FLASH_CR_PG | FLASH_CR_PSIZE_1;
    *(volatile uint32_t *)address = word;
    if (!FlashWait()) { FLASH->CR = 0U; return false; }
    FLASH->CR = 0U;
    address += 4U; data += count; length -= count;
  }
  return true;
}

static bool MetadataValid(const OtaMetadata *metadata)
{
  if (metadata->magic != METADATA_MAGIC || metadata->image_size == 0U ||
      metadata->image_size > (APP_LIMIT - APP_ADDRESS)) return false;
  uint32_t stack = *(volatile uint32_t *)APP_ADDRESS;
  uint32_t reset = *(volatile uint32_t *)(APP_ADDRESS + 4U);
  if (stack < 0x20000000UL || stack > BOOT_REQUEST_ADDRESS || reset < APP_ADDRESS ||
      reset >= APP_LIMIT || (reset & 1U) == 0U) return false;

  uint8_t manifest[12];
  WriteU32(&manifest[0], metadata->image_size);
  WriteU32(&manifest[4], metadata->version);
  WriteU32(&manifest[8], metadata->image_crc32);
  Sha256Context inner, outer;
  HmacInit(&inner, &outer, manifest);
  const uint8_t *image = (const uint8_t *)APP_ADDRESS;
  Sha256_Update(&inner, image, metadata->image_size);
  uint8_t digest[32];
  HmacFinal(&inner, &outer, digest);
  uint32_t crc = Crc32Update(0xFFFFFFFFUL, image, metadata->image_size) ^ 0xFFFFFFFFUL;
  return crc == metadata->image_crc32 && memcmp(digest, metadata->image_hmac, 32U) == 0;
}

__attribute__((noreturn)) static void JumpToApplication(void)
{
  uint32_t stack = *(volatile uint32_t *)APP_ADDRESS;
  uint32_t reset = *(volatile uint32_t *)(APP_ADDRESS + 4U);
  __disable_irq();
  SysTick->CTRL = 0U;
  USART3->CR1 = 0U;
  SCB->VTOR = APP_ADDRESS;
  __set_MSP(stack);
  __enable_irq();
  ((void (*)(void))reset)();
  for (;;) {}
}

static void HandleFrame(uint8_t message, uint8_t sequence, const uint8_t *payload, uint8_t length)
{
  if (message == MSG_OTA_HELLO) {
    uint8_t status[9] = {1U};
    WriteU32(&status[1], APP_LIMIT - APP_ADDRESS);
    WriteU32(&status[5], g_next_offset);
    SendFrame(MSG_OTA_STATUS, sequence, status, sizeof(status));
    return;
  }
  if (message == MSG_OTA_BEGIN) {
    if (length != 44U) { SendAck(message, sequence, STATUS_LENGTH); return; }
    g_image_size = ReadU32(&payload[0]);
    g_image_version = ReadU32(&payload[4]);
    g_expected_crc = ReadU32(&payload[8]);
    if (g_image_size == 0U || g_image_size > (APP_LIMIT - APP_ADDRESS)) {
      SendAck(message, sequence, STATUS_RANGE); return;
    }
    memcpy(g_expected_hmac, &payload[12], 32U);
    FlashUnlock();
    bool ok = FlashEraseSector(4U);
    for (uint32_t sector = 5U; ok && sector <= 7U; ++sector) ok = FlashEraseSector(sector);
    if (!ok) { SendAck(message, sequence, STATUS_FLASH); return; }
    uint8_t manifest[12];
    memcpy(manifest, payload, sizeof(manifest));
    HmacInit(&g_hmac_inner, &g_hmac_outer, manifest);
    g_running_crc = 0xFFFFFFFFUL;
    g_next_offset = 0U;
    g_updating = true;
    SendAck(message, sequence, STATUS_OK);
    return;
  }
  if (message == MSG_OTA_DATA) {
    if (!g_updating) { SendAck(message, sequence, STATUS_STATE); return; }
    if (length < 5U || ReadU32(payload) != g_next_offset ||
        g_next_offset + length - 4U > g_image_size) {
      SendAck(message, sequence, STATUS_RANGE); return;
    }
    uint32_t data_length = length - 4U;
    if (!FlashProgram(APP_ADDRESS + g_next_offset, &payload[4], data_length)) {
      SendAck(message, sequence, STATUS_FLASH); return;
    }
    g_running_crc = Crc32Update(g_running_crc, &payload[4], data_length);
    Sha256_Update(&g_hmac_inner, &payload[4], data_length);
    g_next_offset += data_length;
    SendAck(message, sequence, STATUS_OK);
    return;
  }
  if (message == MSG_OTA_END) {
    if (length != 0U || !g_updating || g_next_offset != g_image_size) {
      SendAck(message, sequence, STATUS_STATE); return;
    }
    uint8_t digest[32];
    HmacFinal(&g_hmac_inner, &g_hmac_outer, digest);
    uint32_t crc = g_running_crc ^ 0xFFFFFFFFUL;
    if (crc != g_expected_crc || memcmp(digest, g_expected_hmac, 32U) != 0) {
      g_updating = false;
      SendAck(message, sequence, STATUS_AUTH);
      return;
    }
    OtaMetadata metadata = {METADATA_MAGIC, g_image_size, g_image_version, g_expected_crc, {0}};
    memcpy(metadata.image_hmac, digest, sizeof(digest));
    if (!FlashProgram(METADATA_ADDRESS, (const uint8_t *)&metadata, sizeof(metadata))) {
      SendAck(message, sequence, STATUS_FLASH); return;
    }
    g_updating = false;
    SendAck(message, sequence, STATUS_OK);
    return;
  }
  if (message == MSG_OTA_ABORT) {
    g_updating = false;
    g_next_offset = 0U;
    SendAck(message, sequence, STATUS_OK);
    return;
  }
  if (message == MSG_OTA_BOOT) {
    if (MetadataValid((const OtaMetadata *)METADATA_ADDRESS)) {
      SendAck(message, sequence, STATUS_OK);
      JumpToApplication();
    }
    SendAck(message, sequence, STATUS_STATE);
    return;
  }
  SendAck(message, sequence, STATUS_UNSUPPORTED);
}

static void ProcessByte(uint8_t byte)
{
  switch (g_parser.state) {
    case 0: g_parser.state = (byte == SYNC0) ? 1U : 0U; break;
    case 1: g_parser.state = (byte == SYNC1) ? 2U : ((byte == SYNC0) ? 1U : 0U); break;
    case 2: g_parser.version = byte; g_parser.state = 3U; break;
    case 3: g_parser.message = byte; g_parser.state = 4U; break;
    case 4: g_parser.sequence = byte; g_parser.state = 5U; break;
    case 5:
      g_parser.length = byte; g_parser.index = 0U;
      g_parser.state = (byte > MAX_PAYLOAD) ? 0U : (byte == 0U ? 7U : 6U);
      break;
    case 6:
      g_parser.payload[g_parser.index++] = byte;
      if (g_parser.index == g_parser.length) g_parser.state = 7U;
      break;
    case 7: g_parser.crc_low = byte; g_parser.state = 8U; break;
    case 8: {
      uint8_t checked[4U + MAX_PAYLOAD] = {g_parser.version, g_parser.message,
                                           g_parser.sequence, g_parser.length};
      memcpy(&checked[4], g_parser.payload, g_parser.length);
      uint16_t received = (uint16_t)g_parser.crc_low | ((uint16_t)byte << 8U);
      uint16_t expected = Crc16(checked, 4U + g_parser.length);
      if (received == expected && g_parser.version == PROTOCOL_VERSION)
        HandleFrame(g_parser.message, g_parser.sequence, g_parser.payload, g_parser.length);
      else
        SendAck(g_parser.message, g_parser.sequence, STATUS_CRC);
      g_parser.state = 0U;
      break;
    }
    default: g_parser.state = 0U; break;
  }
}

int main(void)
{
  bool requested = (*(volatile uint32_t *)BOOT_REQUEST_ADDRESS == BOOT_REQUEST_MAGIC);
  *(volatile uint32_t *)BOOT_REQUEST_ADDRESS = 0U;
  HardwareInit();
  if (!requested && MetadataValid((const OtaMetadata *)METADATA_ADDRESS)) JumpToApplication();
  for (;;) {
    if ((USART3->SR & USART_SR_RXNE) != 0U) ProcessByte((uint8_t)USART3->DR);
  }
}
