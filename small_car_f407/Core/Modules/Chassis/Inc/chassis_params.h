/**
 * @file chassis_params.h
 * @brief 声明可由树莓派运行时调整的底盘参数及其读写接口。
 *
 * 参数值保存在 RAM 中，复位后恢复编译期默认值；树莓派启动时从 YAML 重新下发。
 */
#ifndef CHASSIS_PARAMS_H_
#define CHASSIS_PARAMS_H_

#include <stdbool.h>
#include <stdint.h>

/** 串口协议中稳定的参数编号；新增参数只能追加，不能改变已有编号。 */
typedef enum {
  CHASSIS_PARAM_ODOM_MM_PER_TICK_NUM = 1,
  CHASSIS_PARAM_GAMEPAD_FORWARD_START = 2,
  CHASSIS_PARAM_GAMEPAD_REVERSE_START = 3,
  CHASSIS_PARAM_GAMEPAD_DRIVE_MAX = 4,
  CHASSIS_PARAM_GAMEPAD_TURN_START = 5,
  CHASSIS_PARAM_GAMEPAD_TURN_MAX = 6,
  CHASSIS_PARAM_ULTRA_NEAR_DISTANCE = 7,
  CHASSIS_PARAM_GYRO_LSB_PER_DPS_X10 = 8,
  CHASSIS_PARAM_WHEEL_TRACK_MM = 9,
  CHASSIS_PARAM_YAW_GYRO_WEIGHT_PERMILLE = 10,
  CHASSIS_PARAM_ATTITUDE_GYRO_WEIGHT_PERMILLE = 11,
  CHASSIS_PARAM_IMU_ROLL_OFFSET_MDEG = 12,
  CHASSIS_PARAM_IMU_PITCH_OFFSET_MDEG = 13,
  CHASSIS_PARAM_MAX_LINEAR_SPEED_MM_S = 14,
  CHASSIS_PARAM_MAX_ANGULAR_SPEED_MRAD_S = 15,
  CHASSIS_PARAM_WHEEL_SPEED_CLOSED_LOOP_ENABLED = 16,
  CHASSIS_PARAM_WHEEL_SPEED_KP_X100 = 17,
  CHASSIS_PARAM_WHEEL_SPEED_KI_X100 = 18,
  CHASSIS_PARAM_WHEEL_SPEED_INTEGRAL_LIMIT = 19,
  CHASSIS_PARAM_WHEEL_ACCEL_LIMIT_MM_S2 = 20,
  CHASSIS_PARAM_WHEEL_PWM_MIN = 21,
  CHASSIS_PARAM_WHEEL_LEFT_OUTPUT_PERMILLE = 22,
  CHASSIS_PARAM_WHEEL_RIGHT_OUTPUT_PERMILLE = 23,
} ChassisParamId;

/** 底盘当前运行参数快照；字段名同时对应树莓派 YAML 键名。 */
typedef struct {
  /** 里程计每 tick 毫米比例的分子，分母固定在 odometry.h。 */
  int32_t odom_mm_per_tick_num;
  /** 以下手柄参数均使用 0..1000 归一化尺度。 */
  int16_t gamepad_forward_start;
  int16_t gamepad_reverse_start;
  int16_t gamepad_drive_max;
  int16_t gamepad_turn_start;
  int16_t gamepad_turn_max;
  /** 超声前向强制停车阈值，单位 mm。 */
  int16_t ultra_near_distance_mm;
  /** 陀螺仪灵敏度的 10 倍值，例如 16.4 LSB/dps 保存为 164。 */
  int16_t gyro_lsb_per_dps_x10;
  /** 左右轮中心距，单位 mm。 */
  int16_t wheel_track_mm;
  /** 两个互补滤波权重，单位千分比。 */
  int16_t yaw_gyro_weight_permille;
  int16_t attitude_gyro_weight_permille;
  /** IMU 安装水平误差，单位 mdeg。 */
  int16_t imu_roll_offset_mdeg;
  int16_t imu_pitch_offset_mdeg;
  /** 树莓派物理速度命令限幅，单位 mm/s 和 mrad/s。 */
  int16_t max_linear_speed_mm_s;
  int16_t max_angular_speed_mrad_s;
  /** 轮速闭环开关和 PI 参数；增益使用放大 100 倍的整数表示。 */
  bool wheel_speed_closed_loop_enabled;
  int16_t wheel_speed_kp_x100;
  int16_t wheel_speed_ki_x100;
  int16_t wheel_speed_integral_limit;
  /** 目标轮速斜坡限制，单位 mm/s^2。 */
  int16_t wheel_accel_limit_mm_s2;
  /** 克服电机静摩擦的最小归一化 PWM。 */
  int16_t wheel_pwm_min;
  /** 左右侧输出修正比例，1000 表示不修正。 */
  int16_t wheel_left_output_permille;
  int16_t wheel_right_output_permille;
} ChassisParams;

/** 加载全部编译期默认参数。 */
void ChassisParams_Init(void);
/** 校验并写入一个运行参数；编号或范围非法时返回 false。 */
bool ChassisParams_Set(ChassisParamId id, int32_t value);
/** 按编号读取参数为统一 int32_t 表示。 */
bool ChassisParams_GetValue(ChassisParamId id, int32_t *value);
/** 返回当前完整参数快照。 */
ChassisParams ChassisParams_Get(void);

#endif  // CHASSIS_PARAMS_H_
