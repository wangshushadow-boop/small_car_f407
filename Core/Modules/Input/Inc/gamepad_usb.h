/**
 * @file gamepad_usb.h
 * @brief 声明 USB Host 手柄类驱动和 HID/厂商报告解码入口。
 */
#ifndef GAMEPAD_USB_H_
#define GAMEPAD_USB_H_

#include "usbh_core.h"

/** 标准 HID 接口和 Xbox 风格厂商接口对应的 USB Host 类描述符。 */
extern USBH_ClassTypeDef GamepadUsbHidClass;
extern USBH_ClassTypeDef GamepadUsbVendorHidClass;

/** 解码当前 USB 输入报告并更新 Gamepad 模块状态。 */
USBH_StatusTypeDef GamepadUsb_Decode(USBH_HandleTypeDef *phost);

#endif  // GAMEPAD_USB_H_
