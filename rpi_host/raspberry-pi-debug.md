# 树莓派调试命令

## WiFi 排查

检查 WiFi 是否被禁用：

```bash
rfkill list
```

解除 WiFi 禁用：

```bash
sudo rfkill unblock wifi
rfkill list
```

检查网卡：

```bash
ip link
iw dev
```

检查 NetworkManager：

```bash
nmcli device
```

打开 WiFi 并重启 NetworkManager：

```bash
sudo nmcli radio wifi on
sudo systemctl restart NetworkManager
nmcli device
```

扫描 WiFi：

```bash
nmcli dev wifi list
```

连接 WiFi：

```bash
sudo nmcli dev wifi connect "你的WiFi名称" password "你的WiFi密码"
```

查看 IP：

```bash
ip addr show wlan0
hostname -I
```

查看已有连接：

```bash
nmcli connection show
```

删除旧 WiFi 连接：

```bash
sudo nmcli connection delete "你的WiFi名称"
```

查看 NetworkManager 日志：

```bash
journalctl -u NetworkManager -n 100 --no-pager
```

## 快速收集信息

```bash
rfkill list
nmcli device
nmcli dev wifi list
journalctl -u NetworkManager -n 100 --no-pager
```
