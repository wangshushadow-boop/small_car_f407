#ifndef GAMEPAD_USB_H_
#define GAMEPAD_USB_H_

#include "usbh_core.h"

extern USBH_ClassTypeDef GamepadUsbHidClass;
extern USBH_ClassTypeDef GamepadUsbVendorHidClass;

USBH_StatusTypeDef GamepadUsb_Decode(USBH_HandleTypeDef *phost);

#endif  // GAMEPAD_USB_H_
