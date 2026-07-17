# 树莓派调试命令

## WiFi

```bash
rfkill list
sudo rfkill unblock wifi
ip link
iw dev
nmcli device
sudo nmcli radio wifi on
sudo systemctl restart NetworkManager
nmcli dev wifi list
sudo nmcli dev wifi connect "你的WiFi名称" password "你的WiFi密码"
hostname -I
nmcli connection show
journalctl -u NetworkManager -n 100 --no-pager
```

## 串口

```bash
ls /dev/ttyACM*
lsusb
dmesg | grep -i tty
sudo usermod -aG dialout ubuntu
minicom -D /dev/ttyACM0 -b 115200
```

## 远程传感器日志

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build-sensor-monitor
cmake --build build-sensor-monitor --target sensor_monitor
./build-sensor-monitor/sensor_monitor --port /dev/ttyACM0 --imu --enc --ultra --odom
./build-sensor-monitor/sensor_monitor --port /dev/ttyACM0 --all --interval-ms 300
```

## 相机

```bash
ls /dev/video*
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext
```

## 音频

```bash
arecord -l
aplay -l
arecord -D plughw:0,0 -f S16_LE -r 16000 -d 5 -t wav test.wav
aplay -D plughw:0,0 -f S16_LE -r 16000 -c 1 test.wav
speaker-test -D hw:0,0 -t sine -f 1000 -c 2 -r 48000 -l 1
```

## 构建

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## STM32 烧录

### 连接方式

```text
电脑编译固件 -> scp 发送到树莓派 -> 树莓派通过 ST-LINK 烧录 STM32
```

ST-LINK 需要插在树莓派 USB 口上，SWD 接到 STM32 主控板。

### 树莓派首次安装烧录工具

```bash
sudo apt update
sudo apt install -y stlink-tools
```

### 检查 ST-LINK 是否识别

```bash
lsusb
sudo st-info --probe
```

正常时能看到类似信息：

```text
dev-type: STM32F4x5_F4x7
flash: 524288
```

如果 `st-info --probe` 不加 `sudo` 提示 USB 权限错误，先直接用 `sudo` 烧录即可。

### 电脑端编译固件

在电脑项目目录执行：

```powershell
cmake --build --preset Debug
```

如果只生成了 `.elf`，用 STM32Cube 自带 `objcopy` 生成 `.bin`：

```powershell
& 'C:\Users\10822\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin\arm-none-eabi-objcopy.exe' -O binary 'build\Debug\small_car_f407.elf' 'build\Debug\small_car_f407.bin'
```

### 发送固件到树莓派

```powershell
scp build/Debug/small_car_f407.bin ubuntu@192.168.3.85:~/small_car_f407.bin
```

### 树莓派端烧录

```bash
sudo st-flash --reset write ~/small_car_f407.bin 0x08000000
```

烧录成功时会看到：

```text
Flash written and verified!
```
