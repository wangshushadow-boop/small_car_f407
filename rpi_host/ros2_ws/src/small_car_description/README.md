# 小车 URDF 模型

该模型根据 R3X 底盘参数和实车多视角照片建立，用于 RViz 显示、TF 关系维护和传感器数据定位。

## 坐标系

| 轴 | 正方向 |
| --- | --- |
| x | 小车前方 |
| y | 小车左方 |
| z | 小车上方 |

## 模型结构

| 部件 | 建模依据 |
| --- | --- |
| 四驱底盘 | 厂家尺寸 203.5 mm × 145 mm × 76 mm |
| 四轮和减速电机 | 实车照片，轮胎直径约 65 mm |
| 两层底盘和载荷板 | 实车照片 |
| C30D MCU 主控板和板载 IMU | 85 mm × 58 mm，IMU 水平安装 |
| 树莓派及散热器 | 85 mm × 58 mm，实车照片 |
| 圆形 USB 音响 | 实车照片，直径约 120 mm |
| 双自由度云台和 C100 相机 | 上方舵机横轴驱动侧面呈 L/C 形的摆臂和相机俯仰 |
| 超声与 IMU | 原理图、接口位置和实车照片 |

照片无法提供的尺寸集中定义在 `urdf/robot_geometry.xacro`，后续实测后只需修改该文件。
视觉模型使用简单几何体，不参与 MCU 里程计参数计算。
各模块使用不同颜色区分，四个车轮外侧的红色非对称辐条用于观察转动方向。

## 本地预览

```bash
ros2 launch small_car_description display.launch.py
```

启动后 RViz 固定坐标系为 `base_link`，关节控制窗口可调整水平舵机和俯仰舵机。

只发布模型和 TF，不启动 RViz：

```bash
ros2 launch small_car_description description.launch.py
```
