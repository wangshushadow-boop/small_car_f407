#!/usr/bin/env python3
"""四轮架空时标定左右轮组的空载输出补偿。"""

import statistics
import time

import rclpy
from rcl_interfaces.msg import Parameter, ParameterType, ParameterValue
from rcl_interfaces.srv import GetParameters, SetParameters

from calibrate_wheel_pwm import WheelPwmCalibration


PARAMETER_NAMES = [
    "wheel_speed_kp_x100",
    "wheel_speed_ki_x100",
    "wheel_accel_limit_mm_s2",
    "wheel_left_output_permille",
    "wheel_right_output_permille",
]


def get_parameters(calibration: WheelPwmCalibration) -> dict[str, int]:
    request = GetParameters.Request(names=PARAMETER_NAMES)
    future = calibration.get_client.call_async(request)
    rclpy.spin_until_future_complete(calibration.node, future, timeout_sec=10.0)
    if not future.done() or future.result() is None:
        raise RuntimeError("读取轮速参数超时")
    return {
        name: int(value.integer_value)
        for name, value in zip(PARAMETER_NAMES, future.result().values)
    }


def set_parameters(calibration: WheelPwmCalibration, values: dict[str, int]) -> None:
    request = SetParameters.Request(
        parameters=[
            Parameter(
                name=name,
                value=ParameterValue(
                    type=ParameterType.PARAMETER_INTEGER,
                    integer_value=value,
                ),
            )
            for name, value in values.items()
        ]
    )
    future = calibration.set_client.call_async(request)
    rclpy.spin_until_future_complete(calibration.node, future, timeout_sec=15.0)
    if not future.done() or future.result() is None:
        raise RuntimeError("设置轮速参数超时")
    for result in future.result().results:
        if not result.successful:
            raise RuntimeError(f"设置轮速参数失败: {result.reason}")


def measure(calibration: WheelPwmCalibration, direction: float) -> tuple[float, float]:
    calibration.stop()
    calibration.samples.clear()
    started_at = time.monotonic()
    calibration.publish_for(direction * 0.40, 5.0)
    tail = [sample for sample in calibration.samples if sample[0] - started_at >= 2.5]
    calibration.stop()
    if not tail:
        raise RuntimeError("没有收到轮速样本")
    left = statistics.median(sample[1] for sample in tail)
    right = statistics.median(sample[2] for sample in tail)
    expected_sign = 1.0 if direction > 0 else -1.0
    if left * expected_sign <= 0.0 or right * expected_sign <= 0.0:
        raise RuntimeError(
            f"编码器方向错误: left={left:.2f}, right={right:.2f} rad/s"
        )
    return abs(left), abs(right)


def rounded_compensation(left: float, right: float) -> tuple[int, int]:
    if min(left, right) < 1.0:
        raise RuntimeError("存在未可靠起转的轮组")
    # 空载测量存在轮胎偏摆和编码器量化误差，小于 2% 时保持 1000/1000，
    # 避免最小 1% 参数步进造成反向过补偿。
    if abs(left - right) / max(left, right) <= 0.02:
        return 1000, 1000
    if left < right:
        return min(1200, round(right / left * 100) * 10), 1000
    return 1000, min(1200, round(left / right * 100) * 10)


def main() -> int:
    rclpy.init()
    calibration = WheelPwmCalibration()
    original: dict[str, int] = {}
    nav_paused = False
    try:
        calibration.wait_for_services()
        original = get_parameters(calibration)
        calibration.manage_nav(1)
        nav_paused = True
        print("Nav2 已暂停，开始空载轮组一致性测试", flush=True)

        # 关闭 PI，仅保留物理速度到 PWM 的前馈，避免闭环掩盖左右机械差异。
        set_parameters(
            calibration,
            {
                "wheel_speed_kp_x100": 0,
                "wheel_speed_ki_x100": 0,
                "wheel_accel_limit_mm_s2": 3000,
                "wheel_left_output_permille": 1000,
                "wheel_right_output_permille": 1000,
            },
        )
        calibration.publish_for(0.0, 3.0)
        forward = measure(calibration, 1.0)
        reverse = measure(calibration, -1.0)
        print(
            f"原始速度: 前进 L/R={forward[0]:.2f}/{forward[1]:.2f}, "
            f"后退 L/R={reverse[0]:.2f}/{reverse[1]:.2f} rad/s",
            flush=True,
        )
        left_average = statistics.mean([forward[0], reverse[0]])
        right_average = statistics.mean([forward[1], reverse[1]])
        left_compensation, right_compensation = rounded_compensation(
            left_average, right_average
        )
        set_parameters(
            calibration,
            {
                "wheel_left_output_permille": left_compensation,
                "wheel_right_output_permille": right_compensation,
            },
        )
        checked_forward = measure(calibration, 1.0)
        checked_reverse = measure(calibration, -1.0)
        print(
            f"建议补偿: left={left_compensation}, right={right_compensation}",
            flush=True,
        )
        print(
            f"复测速度: 前进 L/R={checked_forward[0]:.2f}/{checked_forward[1]:.2f}, "
            f"后退 L/R={checked_reverse[0]:.2f}/{checked_reverse[1]:.2f} rad/s",
            flush=True,
        )
        return 0
    finally:
        try:
            calibration.stop()
            if original:
                set_parameters(calibration, original)
                print("已恢复原轮速参数", flush=True)
        finally:
            if nav_paused:
                calibration.manage_nav(2)
                print("Nav2 已恢复", flush=True)
            calibration.node.destroy_node()
            rclpy.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
