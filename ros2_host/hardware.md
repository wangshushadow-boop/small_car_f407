# 树莓派相关硬件文档

只记录树莓派直接接触的设备；STM32 引脚和板级细节见固件文档。

## 设备

| 硬件 | 连接与用途 |
| --- | --- |
| STM32F407 | USB 串口；执行电机闭环、安全任务并上传累计编码器和原始传感器 |
| USB 摄像头 | 通常为 `/dev/video0` |
| Jabra USB 音频 | 麦克风和扬声器 |
| ST-LINK | 可选；通过 SWD 烧录 STM32 |
| 超声、ICM20948、编码器 | 接入 MCU，由 MCU 采集后上传 |

## MCU 串口

| 项目 | 配置 |
| --- | --- |
| 固定设备路径 | `/dev/serial/by-id/usb-1a86_USB_Single_Serial_5C2C059301-if00` |
| 容器路径 | `/dev/small_car_mcu` |
| 参数 | `115200 8N1` |
| 协议 | 二进制协议 v3 |

同一时间只能有一个程序占用串口。运行诊断工具前先停止 Compose。

## 摄像头与音频

```bash
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext
arecord -l
aplay -l
```

## ST-LINK

```bash
sudo apt install -y stlink-tools
sudo st-info --probe
sudo st-flash --reset write small_car_f407.bin 0x08000000
```

发生 timeout 时检查 USB 供电、SWD 接线、目标板供电和工具占用。克隆版 ST-LINK 长文件写入不稳定时，应降低 SWD 频率或按 Flash 扇区分段写入并回读校验。

## URDF 建模

URDF 描述底盘、IMU、超声和云台安装关系，只用于 TF 与可视化，不参与 MCU 控制或里程计参数计算。
