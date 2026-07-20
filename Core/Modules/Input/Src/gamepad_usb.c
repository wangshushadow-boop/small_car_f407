#include "gamepad_usb.h"

#include <stdbool.h>
#include <string.h>

#include "debug_uart.h"
#include "gamepad.h"
#include "usbh_hid.h"

#define WHEELTEC_USB_GAMEPAD_PC_VID 0x0079U
#define WHEELTEC_USB_GAMEPAD_PC_PID 0x0126U
#define WHEELTEC_USB_GAMEPAD_ANDROID_VID 0x045EU
#define WHEELTEC_USB_GAMEPAD_ANDROID_PID 0x028EU
#define GAMEPAD_VENDOR_CLASS_CODE 0xFFU
#define GAMEPAD_REPORT_BUFFER_SIZE 64U
#define GAMEPAD_DEFAULT_AXIS 127U

static USBH_StatusTypeDef GamepadUsb_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef GamepadUsb_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef GamepadUsb_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef GamepadUsb_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef GamepadUsb_SOFProcess(USBH_HandleTypeDef *phost);
static void GamepadUsb_ParseHidDesc(HID_DescTypeDef *desc, uint8_t *buf);
static USBH_StatusTypeDef GamepadUsb_ReportInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef GamepadUsb_DecodeReport(USBH_HandleTypeDef *phost);
typedef enum {
  GAMEPAD_USB_MODE_UNKNOWN = 0,
  GAMEPAD_USB_MODE_PC,
  GAMEPAD_USB_MODE_ANDROID,
} GamepadUsbMode;

static void DecodeWheeltecPcReport(const uint8_t *data);
static void DecodeWheeltecAndroidReport(const uint8_t *data);
static void SetButton(uint16_t *buttons, uint8_t pressed, GamepadButton button);
static bool IsWheeltecUsbGamepad(USBH_HandleTypeDef *phost);
static void ResetGamepad(void);

USBH_ClassTypeDef GamepadUsbHidClass = {
    "GPAD",
    USB_HID_CLASS,
    GamepadUsb_InterfaceInit,
    GamepadUsb_InterfaceDeInit,
    GamepadUsb_ClassRequest,
    GamepadUsb_Process,
    GamepadUsb_SOFProcess,
    NULL,
};

USBH_ClassTypeDef GamepadUsbVendorHidClass = {
    "GPAD",
    GAMEPAD_VENDOR_CLASS_CODE,
    GamepadUsb_InterfaceInit,
    GamepadUsb_InterfaceDeInit,
    GamepadUsb_ClassRequest,
    GamepadUsb_Process,
    GamepadUsb_SOFProcess,
    NULL,
};

static uint8_t g_report_data[GAMEPAD_REPORT_BUFFER_SIZE];
static GamepadUsbMode g_gamepad_mode = GAMEPAD_USB_MODE_UNKNOWN;

static USBH_StatusTypeDef GamepadUsb_InterfaceInit(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status;
  HID_HandleTypeDef *hid_handle;
  uint16_t ep_mps;
  uint8_t max_ep;
  uint8_t interface;

  /*
   * 厂家 USB 接收器有两种模式：
   * - PC 模式：标准 HID class 设备。
   * - Android/XInput 类模式：vendor class 设备。
   * 这里先按 vendor class 查找接口，后面再用 VID/PID 判断是否支持。
   */
  interface = USBH_FindInterface(phost, GAMEPAD_VENDOR_CLASS_CODE, GAMEPAD_VENDOR_CLASS_CODE,
                                 GAMEPAD_VENDOR_CLASS_CODE);
  if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES))
  {
    DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                       "[GAMEPAD] no valid interface, VID=0x%04X PID=0x%04X\r\n",
                       phost->device.DevDesc.idVendor,
                       phost->device.DevDesc.idProduct);
    return USBH_FAIL;
  }

  DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                     "[GAMEPAD] dev VID=0x%04X PID=0x%04X itf=%u class=0x%02X sub=0x%02X proto=0x%02X\r\n",
                     phost->device.DevDesc.idVendor,
                     phost->device.DevDesc.idProduct,
                     interface,
                     phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass,
                     phost->device.CfgDesc.Itf_Desc[interface].bInterfaceSubClass,
                     phost->device.CfgDesc.Itf_Desc[interface].bInterfaceProtocol);

  if (!IsWheeltecUsbGamepad(phost))
  {
    DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                       "[GAMEPAD] unsupported USB device VID=0x%04X PID=0x%04X\r\n",
                       phost->device.DevDesc.idVendor,
                       phost->device.DevDesc.idProduct);
    return USBH_FAIL;
  }

  status = USBH_SelectInterface(phost, interface);
  if (status != USBH_OK)
  {
    return USBH_FAIL;
  }

  phost->pActiveClass->pData = USBH_malloc(sizeof(HID_HandleTypeDef));
  hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;
  if (hid_handle == NULL)
  {
    USBH_DbgLog("Cannot allocate memory for gamepad HID handle.");
    return USBH_FAIL;
  }

  /* 复用 ST HID_HandleTypeDef，让 USB Host 状态机仍然按 HID 中断端点收包。 */
  (void)USBH_memset(hid_handle, 0, sizeof(HID_HandleTypeDef));
  hid_handle->state = USBH_HID_INIT;
  hid_handle->ctl_state = USBH_HID_REQ_INIT;
  hid_handle->Init = GamepadUsb_ReportInit;
  hid_handle->ep_addr = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
  hid_handle->length = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
  hid_handle->poll = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bInterval;
  if (hid_handle->poll < HID_MIN_POLL)
  {
    hid_handle->poll = HID_MIN_POLL;
  }

  max_ep = (phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS)
               ? phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints
               : USBH_MAX_NUM_ENDPOINTS;

  DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                     "[GAMEPAD] ep0=0x%02X len=%u poll=%u max_ep=%u\r\n",
                     hid_handle->ep_addr,
                     hid_handle->length,
                     hid_handle->poll,
                     max_ep);

  for (uint8_t num = 0U; num < max_ep; ++num)
  {
    /* 找到 IN/OUT 中断端点。当前主要使用 IN 端点读取手柄报文。 */
    const uint8_t ep_addr = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress;
    ep_mps = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].wMaxPacketSize;
    if ((ep_addr & 0x80U) != 0U)
    {
      hid_handle->InEp = ep_addr;
      hid_handle->InPipe = USBH_AllocPipe(phost, hid_handle->InEp);
      (void)USBH_OpenPipe(phost, hid_handle->InPipe, hid_handle->InEp, phost->device.address,
                          phost->device.speed, USB_EP_TYPE_INTR, ep_mps);
      (void)USBH_LL_SetToggle(phost, hid_handle->InPipe, 0U);
    }
    else
    {
      hid_handle->OutEp = ep_addr;
      hid_handle->OutPipe = USBH_AllocPipe(phost, hid_handle->OutEp);
      (void)USBH_OpenPipe(phost, hid_handle->OutPipe, hid_handle->OutEp, phost->device.address,
                          phost->device.speed, USB_EP_TYPE_INTR, ep_mps);
      (void)USBH_LL_SetToggle(phost, hid_handle->OutPipe, 0U);
    }
  }

  DebugUart_PrintfIf(DEBUG_LOG_GAMEPAD,
                     "[GAMEPAD] WHEELTEC USB receiver detected, mode=%s\r\n",
                     g_gamepad_mode == GAMEPAD_USB_MODE_ANDROID ? "android" : "pc");
  return USBH_OK;
}

static USBH_StatusTypeDef GamepadUsb_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
  /* USB 断开时释放管道和 HID 句柄，并把手柄状态复位为未连接。 */
  HID_HandleTypeDef *hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;

  if (hid_handle != NULL)
  {
    if (hid_handle->InPipe != 0x00U)
    {
      (void)USBH_ClosePipe(phost, hid_handle->InPipe);
      (void)USBH_FreePipe(phost, hid_handle->InPipe);
      hid_handle->InPipe = 0U;
    }

    if (hid_handle->OutPipe != 0x00U)
    {
      (void)USBH_ClosePipe(phost, hid_handle->OutPipe);
      (void)USBH_FreePipe(phost, hid_handle->OutPipe);
      hid_handle->OutPipe = 0U;
    }

    USBH_free(phost->pActiveClass->pData);
    phost->pActiveClass->pData = NULL;
  }

  ResetGamepad();
  return USBH_OK;
}

static USBH_StatusTypeDef GamepadUsb_ClassRequest(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_BUSY;
  USBH_StatusTypeDef class_req_status;
  HID_HandleTypeDef *hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;

  switch (hid_handle->ctl_state)
  {
    case USBH_HID_REQ_INIT:
    case USBH_HID_REQ_GET_HID_DESC:
      /* 从配置描述符里解析 HID 描述符，获取报告描述符长度。 */
      GamepadUsb_ParseHidDesc(&hid_handle->HID_Desc, phost->device.CfgDesc_Raw);
      hid_handle->ctl_state = USBH_HID_REQ_GET_REPORT_DESC;
      break;

    case USBH_HID_REQ_GET_REPORT_DESC:
      class_req_status = USBH_HID_GetHIDReportDescriptor(phost, hid_handle->HID_Desc.wItemLength);
      if (class_req_status == USBH_OK)
      {
        hid_handle->ctl_state = USBH_HID_REQ_SET_IDLE;
      }
      else if (class_req_status == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Gamepad HID report descriptor request failed.");
        status = USBH_FAIL;
      }
      break;

    case USBH_HID_REQ_SET_IDLE:
      class_req_status = USBH_HID_SetIdle(phost, 0U, 0U);
      if ((class_req_status == USBH_OK) || (class_req_status == USBH_NOT_SUPPORTED))
      {
        hid_handle->ctl_state = USBH_HID_REQ_SET_PROTOCOL;
      }
      break;

    case USBH_HID_REQ_SET_PROTOCOL:
      class_req_status = USBH_HID_SetProtocol(phost, 0U);
      if ((class_req_status == USBH_OK) || (class_req_status == USBH_NOT_SUPPORTED))
      {
        /* 类请求完成后通知 USB Host：当前类已经可用。 */
        hid_handle->ctl_state = USBH_HID_REQ_IDLE;
        phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
        status = USBH_OK;
      }
      break;

    case USBH_HID_REQ_IDLE:
    default:
      break;
  }

  return status;
}

static USBH_StatusTypeDef GamepadUsb_Process(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_OK;
  HID_HandleTypeDef *hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;

  switch (hid_handle->state)
  {
    case USBH_HID_INIT:
      status = hid_handle->Init(phost);
      hid_handle->state = (status == USBH_OK) ? USBH_HID_IDLE : USBH_HID_ERROR;
#if (USBH_USE_OS == 1U)
      USBH_OS_PutMessage(phost, USBH_URB_EVENT, 0U, 0U);
#endif
      break;

    case USBH_HID_IDLE:
      status = USBH_HID_GetReport(phost, 0x01U, 0U, hid_handle->pData,
                                  (uint8_t)hid_handle->length);
      if (status == USBH_OK)
      {
        hid_handle->state = USBH_HID_SYNC;
      }
      else if ((status == USBH_BUSY) || (status == USBH_NOT_SUPPORTED))
      {
        hid_handle->state = (status == USBH_BUSY) ? USBH_HID_IDLE : USBH_HID_SYNC;
        status = USBH_OK;
      }
      else
      {
        hid_handle->state = USBH_HID_ERROR;
        status = USBH_FAIL;
      }
#if (USBH_USE_OS == 1U)
      USBH_OS_PutMessage(phost, USBH_URB_EVENT, 0U, 0U);
#endif
      break;

    case USBH_HID_SYNC:
      if ((phost->Timer & 1U) != 0U)
      {
        hid_handle->state = USBH_HID_GET_DATA;
      }
#if (USBH_USE_OS == 1U)
      USBH_OS_PutMessage(phost, USBH_URB_EVENT, 0U, 0U);
#endif
      break;

    case USBH_HID_GET_DATA:
      /* 发起一次中断 IN 传输，等待手柄报告数据。 */
      (void)USBH_InterruptReceiveData(phost, hid_handle->pData, (uint8_t)hid_handle->length,
                                      hid_handle->InPipe);
      hid_handle->state = USBH_HID_POLL;
      hid_handle->timer = phost->Timer;
      hid_handle->DataReady = 0U;
      break;

    case USBH_HID_POLL:
      if (USBH_LL_GetURBState(phost, hid_handle->InPipe) == USBH_URB_DONE)
      {
        /* 收到完整报文后写入 FIFO，再触发解码回调。 */
        uint32_t xfer_size = USBH_LL_GetLastXferSize(phost, hid_handle->InPipe);
        if ((hid_handle->DataReady == 0U) && (xfer_size != 0U) && (hid_handle->fifo.buf != NULL))
        {
          (void)USBH_HID_FifoWrite(&hid_handle->fifo, hid_handle->pData, hid_handle->length);
          hid_handle->DataReady = 1U;
          USBH_HID_EventCallback(phost);
#if (USBH_USE_OS == 1U)
          USBH_OS_PutMessage(phost, USBH_URB_EVENT, 0U, 0U);
#endif
        }
      }
      else if (USBH_LL_GetURBState(phost, hid_handle->InPipe) == USBH_URB_STALL)
      {
        if (USBH_ClrFeature(phost, hid_handle->ep_addr) == USBH_OK)
        {
          hid_handle->state = USBH_HID_GET_DATA;
        }
      }
      break;

    default:
      break;
  }

  return status;
}

static USBH_StatusTypeDef GamepadUsb_SOFProcess(USBH_HandleTypeDef *phost)
{
  /* 按端点 bInterval 控制轮询频率，避免过快重复提交 IN 传输。 */
  HID_HandleTypeDef *hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;
  if ((hid_handle != NULL) && (hid_handle->state == USBH_HID_POLL) &&
      ((phost->Timer - hid_handle->timer) >= hid_handle->poll))
  {
    hid_handle->state = USBH_HID_GET_DATA;
#if (USBH_USE_OS == 1U)
    USBH_OS_PutMessage(phost, USBH_URB_EVENT, 0U, 0U);
#endif
  }
  return USBH_OK;
}

static void GamepadUsb_ParseHidDesc(HID_DescTypeDef *desc, uint8_t *buf)
{
  /* 在配置描述符中查找 HID 描述符；部分手柄类请求会用到报告描述符长度。 */
  USBH_DescHeader_t *pdesc = (USBH_DescHeader_t *)buf;
  uint16_t cfg_desc_len = LE16(buf + 2U);
  uint16_t ptr = USB_LEN_CFG_DESC;

  memset(desc, 0, sizeof(*desc));
  while (ptr < cfg_desc_len)
  {
    pdesc = USBH_GetNextDesc((uint8_t *)pdesc, &ptr);
    if (pdesc->bDescriptorType == USB_DESC_TYPE_HID)
    {
      desc->bLength = *((uint8_t *)pdesc + 0U);
      desc->bDescriptorType = *((uint8_t *)pdesc + 1U);
      desc->bcdHID = LE16((uint8_t *)pdesc + 2U);
      desc->bCountryCode = *((uint8_t *)pdesc + 4U);
      desc->bNumDescriptors = *((uint8_t *)pdesc + 5U);
      desc->bReportDescriptorType = *((uint8_t *)pdesc + 6U);
      desc->wItemLength = LE16((uint8_t *)pdesc + 7U);
      break;
    }
  }
}

static USBH_StatusTypeDef GamepadUsb_ReportInit(USBH_HandleTypeDef *phost)
{
  /* 报文缓冲区固定为 64 字节，超过则截断，防止越界。 */
  HID_HandleTypeDef *hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;
  if (hid_handle->length > GAMEPAD_REPORT_BUFFER_SIZE)
  {
    hid_handle->length = GAMEPAD_REPORT_BUFFER_SIZE;
  }

  hid_handle->pData = g_report_data;
  if ((HID_QUEUE_SIZE * GAMEPAD_REPORT_BUFFER_SIZE) > sizeof(phost->device.Data))
  {
    return USBH_FAIL;
  }

  USBH_HID_FifoInit(&hid_handle->fifo, phost->device.Data,
                    (uint16_t)(HID_QUEUE_SIZE * GAMEPAD_REPORT_BUFFER_SIZE));
  return USBH_OK;
}

USBH_StatusTypeDef GamepadUsb_Decode(USBH_HandleTypeDef *phost)
{
  return GamepadUsb_DecodeReport(phost);
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
  (void)GamepadUsb_DecodeReport(phost);
}

static USBH_StatusTypeDef GamepadUsb_DecodeReport(USBH_HandleTypeDef *phost)
{
  /* 从 HID FIFO 中取出一帧报告，然后根据接收器模式选择不同解码方式。 */
  HID_HandleTypeDef *hid_handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;
  if ((hid_handle == NULL) || (hid_handle->length == 0U) || (hid_handle->fifo.buf == NULL))
  {
    return USBH_FAIL;
  }

  if (USBH_HID_FifoRead(&hid_handle->fifo, g_report_data, hid_handle->length) !=
      hid_handle->length)
  {
    return USBH_FAIL;
  }

  if (g_gamepad_mode == GAMEPAD_USB_MODE_PC)
  {
    DecodeWheeltecPcReport(g_report_data);
  }
  else if (g_gamepad_mode == GAMEPAD_USB_MODE_ANDROID)
  {
    DecodeWheeltecAndroidReport(g_report_data);
  }
  else
  {
    return USBH_FAIL;
  }
  return USBH_OK;
}

static void DecodeWheeltecPcReport(const uint8_t *data)
{
  /*
   * PC 模式报文：
   * - data[0]/data[1] 保存大部分按键位。
   * - data[2] 低 4 位是方向帽。
   * - data[3..6] 是 LX/LY/RX/RY。
   */
  uint16_t buttons = 0U;

  SetButton(&buttons, (data[1] >> 0) & 0x01U, GAMEPAD_BUTTON_SELECT);
  SetButton(&buttons, (data[1] >> 1) & 0x01U, GAMEPAD_BUTTON_START);
  SetButton(&buttons, (data[1] >> 2) & 0x01U, GAMEPAD_BUTTON_LEFT_STICK);
  SetButton(&buttons, (data[1] >> 3) & 0x01U, GAMEPAD_BUTTON_RIGHT_STICK);
  SetButton(&buttons, (data[0] >> 4) & 0x01U, GAMEPAD_BUTTON_L1);
  SetButton(&buttons, (data[0] >> 5) & 0x01U, GAMEPAD_BUTTON_R1);
  SetButton(&buttons, (data[0] >> 6) & 0x01U, GAMEPAD_BUTTON_L2);
  SetButton(&buttons, (data[0] >> 7) & 0x01U, GAMEPAD_BUTTON_R2);
  SetButton(&buttons, (data[0] >> 0) & 0x01U, GAMEPAD_BUTTON_GREEN);
  SetButton(&buttons, (data[0] >> 1) & 0x01U, GAMEPAD_BUTTON_RED);
  SetButton(&buttons, (data[0] >> 2) & 0x01U, GAMEPAD_BUTTON_BLUE);
  SetButton(&buttons, (data[0] >> 3) & 0x01U, GAMEPAD_BUTTON_PINK);

  uint8_t hat = data[2] & 0x0FU;
  if (hat != 0x0FU)
  {
    /* 方向帽支持斜向，这里拆成上下左右四个独立按键。 */
    const uint8_t diagonal = hat & 0x01U;
    const uint8_t direction = (hat >> 1) & 0x03U;
    SetButton(&buttons, (direction == 0U) || (diagonal && (direction == 3U)), GAMEPAD_BUTTON_UP);
    SetButton(&buttons, (direction == 1U) || (diagonal && (direction == 0U)), GAMEPAD_BUTTON_RIGHT);
    SetButton(&buttons, (direction == 2U) || (diagonal && (direction == 1U)), GAMEPAD_BUTTON_DOWN);
    SetButton(&buttons, (direction == 3U) || (diagonal && (direction == 2U)), GAMEPAD_BUTTON_LEFT);
  }

  Gamepad_UpdateFromUsb(true, data[3], data[4], data[5], data[6], buttons);
}

static void DecodeWheeltecAndroidReport(const uint8_t *data)
{
  /*
   * Android/XInput 类模式下，部分摇杆空闲值会读到 0。
   * 如果高低字节都为 0，则按中心值 128 处理，避免误判为打满方向。
   */
  const uint8_t lx = ((data[6] == 0U) && (data[7] == 0U)) ? 128U : data[6];
  const uint8_t ly = 255U - (((data[8] == 0U) && (data[9] == 0U)) ? 128U : data[8]);
  const uint8_t rx = ((data[10] == 0U) && (data[11] == 0U)) ? 128U : data[10];
  const uint8_t ry = 255U - (((data[12] == 0U) && (data[13] == 0U)) ? 128U : data[12]);
  uint16_t buttons = 0U;

  SetButton(&buttons, (data[2] >> 0) & 0x01U, GAMEPAD_BUTTON_UP);
  SetButton(&buttons, (data[2] >> 3) & 0x01U, GAMEPAD_BUTTON_RIGHT);
  SetButton(&buttons, (data[2] >> 1) & 0x01U, GAMEPAD_BUTTON_DOWN);
  SetButton(&buttons, (data[2] >> 2) & 0x01U, GAMEPAD_BUTTON_LEFT);
  SetButton(&buttons, (data[2] >> 5) & 0x01U, GAMEPAD_BUTTON_SELECT);
  SetButton(&buttons, (data[2] >> 4) & 0x01U, GAMEPAD_BUTTON_START);
  SetButton(&buttons, (data[2] >> 6) & 0x01U, GAMEPAD_BUTTON_LEFT_STICK);
  SetButton(&buttons, (data[2] >> 7) & 0x01U, GAMEPAD_BUTTON_RIGHT_STICK);
  SetButton(&buttons, (data[3] >> 0) & 0x01U, GAMEPAD_BUTTON_L1);
  SetButton(&buttons, (data[3] >> 1) & 0x01U, GAMEPAD_BUTTON_R1);
  SetButton(&buttons, data[4] == 0xFFU, GAMEPAD_BUTTON_L2);
  SetButton(&buttons, data[5] == 0xFFU, GAMEPAD_BUTTON_R2);
  SetButton(&buttons, (data[3] >> 7) & 0x01U, GAMEPAD_BUTTON_GREEN);
  SetButton(&buttons, (data[3] >> 5) & 0x01U, GAMEPAD_BUTTON_RED);
  SetButton(&buttons, (data[3] >> 4) & 0x01U, GAMEPAD_BUTTON_BLUE);
  SetButton(&buttons, (data[3] >> 6) & 0x01U, GAMEPAD_BUTTON_PINK);

  Gamepad_UpdateFromUsb(true, lx, ly, rx, ry, buttons);
}

static void SetButton(uint16_t *buttons, uint8_t pressed, GamepadButton button)
{
  /* 把不同报文里的按键位统一映射到 GamepadButton 位图。 */
  if (pressed != 0U)
  {
    *buttons |= (uint16_t)(1UL << (uint8_t)button);
  }
  else
  {
    *buttons &= (uint16_t)~(1UL << (uint8_t)button);
  }
}

static bool IsWheeltecUsbGamepad(USBH_HandleTypeDef *phost)
{
  /* 只支持当前项目验证过的厂家 USB 接收器 VID/PID。 */
  if ((phost->device.DevDesc.idVendor == WHEELTEC_USB_GAMEPAD_PC_VID) &&
      (phost->device.DevDesc.idProduct == WHEELTEC_USB_GAMEPAD_PC_PID))
  {
    g_gamepad_mode = GAMEPAD_USB_MODE_PC;
    return true;
  }

  if ((phost->device.DevDesc.idVendor == WHEELTEC_USB_GAMEPAD_ANDROID_VID) &&
      (phost->device.DevDesc.idProduct == WHEELTEC_USB_GAMEPAD_ANDROID_PID))
  {
    g_gamepad_mode = GAMEPAD_USB_MODE_ANDROID;
    return true;
  }

  g_gamepad_mode = GAMEPAD_USB_MODE_UNKNOWN;
  return false;
}

static void ResetGamepad(void)
{
  /* USB 断开后恢复为未连接和摇杆居中，避免保留最后一次控制量。 */
  g_gamepad_mode = GAMEPAD_USB_MODE_UNKNOWN;
  Gamepad_UpdateFromUsb(false,
                        GAMEPAD_DEFAULT_AXIS,
                        GAMEPAD_DEFAULT_AXIS,
                        GAMEPAD_DEFAULT_AXIS,
                        GAMEPAD_DEFAULT_AXIS,
                        0U);
}
