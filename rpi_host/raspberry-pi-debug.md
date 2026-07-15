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
