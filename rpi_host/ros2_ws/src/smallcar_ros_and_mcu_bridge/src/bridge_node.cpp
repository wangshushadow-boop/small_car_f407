/**
 * @file bridge_node.cpp
 * @brief 实现 ROS 2 消息与 STM32 串口协议之间的双向桥接节点。
 *
 * 节点负责底盘参数下发、速度和舵机命令转发、传感器消息发布、TF 发布以及
 * 串口故障诊断；运动规划和速度平滑由独立运动控制节点完成。
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/u_int8.hpp>
#include <std_srvs/srv/empty.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <trajectory_msgs/msg/joint_trajectory.hpp>

#include "small_car_host/car_client.hpp"
#include "small_car_host/chassis_config.hpp"

namespace small_car_ros {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kGravity = 9.80665;

/** 将 roll、pitch、yaw 欧拉角转换为 ROS 使用的单位四元数。 */
geometry_msgs::msg::Quaternion QuaternionFromRpy(double roll, double pitch, double yaw) {
  const double cr = std::cos(roll * 0.5);
  const double sr = std::sin(roll * 0.5);
  const double cp = std::cos(pitch * 0.5);
  const double sp = std::sin(pitch * 0.5);
  const double cy = std::cos(yaw * 0.5);
  const double sy = std::sin(yaw * 0.5);

  geometry_msgs::msg::Quaternion result;
  result.w = cr * cp * cy + sr * sp * sy;
  result.x = sr * cp * cy - cr * sp * sy;
  result.y = cr * sp * cy + sr * cp * sy;
  result.z = cr * cp * sy - sr * sp * cy;
  return result;
}

/** 构造一项 diagnostic_msgs 键值，减少诊断发布代码的重复。 */
diagnostic_msgs::msg::KeyValue DiagnosticValue(const std::string& key,
                                                const std::string& value) {
  diagnostic_msgs::msg::KeyValue result;
  result.key = key;
  result.value = value;
  return result;
}

}  // namespace

/**
 * ROS 2 与 MCU 的唯一串口桥接节点。
 *
 * 节点拥有串口设备，并把 MCU 的整数协议单位转换为 ROS SI 单位。其它节点不应
 * 直接打开同一串口，以免两进程分走字节导致协议帧损坏。
 */
class SmallcarRosAndMcuBridge : public rclcpp::Node {
 public:
  /** 按依赖顺序初始化：参数 -> 串口 -> ROS 接口 -> MCU 参数 -> 遥测开关。 */
  SmallcarRosAndMcuBridge() : Node("smallcar_ros_and_mcu_bridge") {
    // 构造阶段只做一次性初始化：读取 ROS 参数、打开串口、下发底盘参数、创建话题和定时器。
    DeclareParameters();
    ReadParameters();
    OpenController();
    CreateRosInterfaces();
    ApplyControllerConfig();
    ConfigureTelemetry();
    RCLCPP_INFO(get_logger(), "ROS2 bridge ready: %s @ %d", serial_port_.c_str(),
                baud_rate_);
  }

  ~SmallcarRosAndMcuBridge() override {
    // 节点退出时主动停车，避免进程异常结束后 MCU 继续保持最后一次运动命令。
    if (client_.IsOpen()) {
      client_.SendStop();
      client_.Close();
    }
  }

 private:
  /** 单个舵机从 ROS 弧度到 MCU PWM 脉宽的线性映射参数。 */
  struct ServoMapping {
    int center_us = 1500;
    int min_us = 800;
    int max_us = 2200;
    double range_rad = kPi;
    double sign = 1.0;
    double commanded_rad = 0.0;
  };

  /** 声明串口、话题、传感器、协方差和舵机映射参数。 */
  void DeclareParameters() {
    declare_parameter<std::string>("serial_port", "/dev/small_car_mcu");
    declare_parameter<int>("baud_rate", 115200);
    declare_parameter<std::string>("chassis_config", "");
    declare_parameter<std::string>("cmd_vel_topic", "cmd_vel_mcu");
    declare_parameter<double>("max_linear_speed_mps", 0.6);
    declare_parameter<double>("max_angular_speed_rad_s", 2.0);
    declare_parameter<int>("cmd_vel_timeout_ms", 500);
    declare_parameter<double>("command_rate_hz", 20.0);
    declare_parameter<std::string>(
        "mcu_recovery_request",
        "/workspace/rpi_host/runtime/mcu_recovery.request");
    declare_parameter<std::string>("odom_frame", "odom");
    declare_parameter<std::string>("base_frame", "base_link");
    declare_parameter<std::string>("imu_frame", "imu_link");
    declare_parameter<std::string>("ultrasonic_frame", "ultrasonic_link");
    declare_parameter<bool>("publish_tf", true);
    declare_parameter<bool>("publish_imu_raw", false);
    declare_parameter<bool>("publish_joint_states", true);
    declare_parameter<double>("wheel_radius_m", 0.0325);
    declare_parameter<double>("ultrasonic_min_range_m", 0.02);
    declare_parameter<double>("ultrasonic_max_range_m", 4.0);
    declare_parameter<double>("ultrasonic_field_of_view_rad", 0.52);
    declare_parameter<double>("odom_position_variance", 0.01);
    declare_parameter<double>("odom_orientation_variance", 0.02);
    declare_parameter<double>("odom_linear_velocity_variance", 0.04);
    declare_parameter<double>("odom_angular_velocity_variance", 0.05);
    declare_parameter<double>("imu_acceleration_variance", 0.1);
    declare_parameter<double>("imu_angular_velocity_variance", 0.02);
    declare_parameter<double>("imu_orientation_variance", 0.02);
    DeclareServoParameters("left", 1500, 800, 2300);
    DeclareServoParameters("right", 1250, 800, 1700);
  }

  /** 为左右舵机声明一组同结构参数，前缀由 name 区分。 */
  void DeclareServoParameters(const std::string& name, int center, int minimum,
                              int maximum) {
    declare_parameter<int>(name + "_servo_center_us", center);
    declare_parameter<int>(name + "_servo_min_us", minimum);
    declare_parameter<int>(name + "_servo_max_us", maximum);
    declare_parameter<double>(name + "_servo_range_rad", kPi);
    declare_parameter<double>(name + "_servo_sign", 1.0);
  }

  /** 启动时读取全部参数，并验证影响单位换算的比例值必须为正。 */
  void ReadParameters() {
    serial_port_ = get_parameter("serial_port").as_string();
    baud_rate_ = static_cast<int>(get_parameter("baud_rate").as_int());
    chassis_config_ = get_parameter("chassis_config").as_string();
    cmd_vel_topic_ = get_parameter("cmd_vel_topic").as_string();
    if (chassis_config_.empty()) {
      chassis_config_ =
          ament_index_cpp::get_package_share_directory("smallcar_ros_and_mcu_bridge") +
          "/config/chassis_params.yaml";
    }
    max_linear_speed_mps_ = get_parameter("max_linear_speed_mps").as_double();
    max_angular_speed_rad_s_ = get_parameter("max_angular_speed_rad_s").as_double();
    cmd_vel_timeout_ =
        std::chrono::milliseconds(get_parameter("cmd_vel_timeout_ms").as_int());
    command_rate_hz_ = get_parameter("command_rate_hz").as_double();
    mcu_recovery_request_ = get_parameter("mcu_recovery_request").as_string();
    odom_frame_ = get_parameter("odom_frame").as_string();
    base_frame_ = get_parameter("base_frame").as_string();
    imu_frame_ = get_parameter("imu_frame").as_string();
    ultrasonic_frame_ = get_parameter("ultrasonic_frame").as_string();
    publish_tf_ = get_parameter("publish_tf").as_bool();
    publish_imu_raw_ = get_parameter("publish_imu_raw").as_bool();
    publish_joint_states_ = get_parameter("publish_joint_states").as_bool();
    wheel_radius_m_ = get_parameter("wheel_radius_m").as_double();
    ultra_min_m_ = get_parameter("ultrasonic_min_range_m").as_double();
    ultra_max_m_ = get_parameter("ultrasonic_max_range_m").as_double();
    ultra_fov_rad_ = get_parameter("ultrasonic_field_of_view_rad").as_double();
    odom_position_variance_ = get_parameter("odom_position_variance").as_double();
    odom_orientation_variance_ = get_parameter("odom_orientation_variance").as_double();
    odom_linear_velocity_variance_ =
        get_parameter("odom_linear_velocity_variance").as_double();
    odom_angular_velocity_variance_ =
        get_parameter("odom_angular_velocity_variance").as_double();
    imu_acceleration_variance_ = get_parameter("imu_acceleration_variance").as_double();
    imu_angular_velocity_variance_ =
        get_parameter("imu_angular_velocity_variance").as_double();
    imu_orientation_variance_ = get_parameter("imu_orientation_variance").as_double();
    left_servo_ = ReadServoParameters("left");
    right_servo_ = ReadServoParameters("right");

    if (max_linear_speed_mps_ <= 0.0 || max_angular_speed_rad_s_ <= 0.0 ||
        command_rate_hz_ <= 0.0 || wheel_radius_m_ <= 0.0) {
      throw std::runtime_error("ROS2 bridge contains a non-positive scale parameter");
    }
  }

  /** 读取一组舵机参数并检查 min <= center <= max。 */
  ServoMapping ReadServoParameters(const std::string& name) {
    ServoMapping result;
    result.center_us = static_cast<int>(get_parameter(name + "_servo_center_us").as_int());
    result.min_us = static_cast<int>(get_parameter(name + "_servo_min_us").as_int());
    result.max_us = static_cast<int>(get_parameter(name + "_servo_max_us").as_int());
    result.range_rad = get_parameter(name + "_servo_range_rad").as_double();
    result.sign = get_parameter(name + "_servo_sign").as_double();
    if (result.min_us > result.center_us || result.center_us > result.max_us ||
        result.range_rad <= 0.0) {
      throw std::runtime_error("invalid " + name + " servo mapping");
    }
    return result;
  }

  /** 打开桥接节点独占的 MCU 串口，失败时阻止节点继续启动。 */
  void OpenController() {
    if (!client_.Open(serial_port_, baud_rate_)) {
      throw std::runtime_error("cannot open serial port: " + serial_port_);
    }
  }

  /** 从 YAML 加载参数并逐项写入 MCU；失败时保留桥接功能用于现场排查。 */
  void ApplyControllerConfig() {
    const auto parameters = small_car::LoadChassisConfig(chassis_config_);
    std::string error;
    if (!small_car::ApplyChassisConfig(&client_, parameters,
                                       std::chrono::milliseconds(500), &error)) {
      /*
       * 参数下发失败不能阻止 ROS2 bridge 启动。
       * 串口链路偶尔会因为 MCU 正在连续上报数据而错过参数回读，此时传感器发布、
       * cmd_vel 控制和后续手动重试仍然应该可用。
       */
      RCLCPP_WARN(get_logger(), "chassis config not verified: %s", error.c_str());
      return;
    }
    RCLCPP_INFO(get_logger(), "applied and verified %zu chassis parameters",
                parameters.size());
  }

  /**
   * 创建全部 ROS 发布、订阅、服务和定时器。
   *
   * 5 ms 定时器负责尽快清空串口上行数据；command_rate_hz 定时器负责维持
   * 下行速度或心跳，两者分开避免传感器处理拖慢安全停车。
   */
  void CreateRosInterfaces() {
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom", 10);
    if (publish_imu_raw_) {
      imu_raw_pub_ = create_publisher<sensor_msgs::msg::Imu>(
          "imu/data_raw", rclcpp::SensorDataQoS());
    }
    imu_pub_ = create_publisher<sensor_msgs::msg::Imu>(
        "imu/data", rclcpp::SensorDataQoS());
    range_pub_ = create_publisher<sensor_msgs::msg::Range>(
        "ultrasonic/front", rclcpp::SensorDataQoS());
    if (publish_joint_states_) {
      joint_pub_ =
          create_publisher<sensor_msgs::msg::JointState>("joint_states", 10);
    }
    diagnostics_pub_ =
        create_publisher<diagnostic_msgs::msg::DiagnosticArray>("diagnostics", 10);
    control_source_pub_ = create_publisher<std_msgs::msg::UInt8>(
        "control/source", rclcpp::QoS(1).reliable().transient_local());
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        cmd_vel_topic_, 10,
        std::bind(&SmallcarRosAndMcuBridge::OnCmdVel, this, std::placeholders::_1));
    servo_sub_ = create_subscription<trajectory_msgs::msg::JointTrajectory>(
        "servo_controller/joint_trajectory", 10,
        std::bind(&SmallcarRosAndMcuBridge::OnServoTrajectory, this,
                  std::placeholders::_1));
    reset_odom_service_ = create_service<std_srvs::srv::Empty>(
        "reset_odometry",
        std::bind(&SmallcarRosAndMcuBridge::OnResetOdometry, this,
                  std::placeholders::_1,
                  std::placeholders::_2));

    poll_timer_ = create_wall_timer(std::chrono::milliseconds(5),
                                    std::bind(&SmallcarRosAndMcuBridge::PollController, this));
    const auto period = std::chrono::duration<double>(1.0 / command_rate_hz_);
    command_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&SmallcarRosAndMcuBridge::MaintainCommand, this));
  }

  /** 根据启用的 ROS 发布项配置 MCU，关闭无消费者的周期遥测以节省串口带宽。 */
  void ConfigureTelemetry() {
    std::uint16_t mask =
        small_car::kTelemetryChassis | small_car::kTelemetryImu |
        small_car::kTelemetryDevice | small_car::kTelemetryOdometry;
    if (publish_joint_states_) {
      mask |= small_car::kTelemetryOdometryDebug;
    }
    if (!client_.SendTelemetryConfig(mask)) {
      RCLCPP_WARN(get_logger(), "failed to configure MCU telemetry");
    }
  }

  /** 接收平滑后的速度，限幅并转换为 mm/s、mrad/s 后立即发送。 */
  void OnCmdVel(const geometry_msgs::msg::Twist::SharedPtr message) {
    linear_command_mm_s_ = ToMilliUnits(message->linear.x, max_linear_speed_mps_);
    angular_command_mrad_s_ =
        ToMilliUnits(message->angular.z, max_angular_speed_rad_s_);
    last_cmd_vel_time_ = std::chrono::steady_clock::now();
    have_cmd_vel_ = true;
    last_control_send_ok_ =
        client_.SendDrive(linear_command_mm_s_, angular_command_mrad_s_);
    if (!last_control_send_ok_) {
      RequestMcuRecovery();
    }
  }

  /** 将 SI 单位浮点数限幅并转换为协议使用的千分之一单位。 */
  static std::int16_t ToMilliUnits(double value, double maximum) {
    const double limited = std::clamp(value, -maximum, maximum);
    return static_cast<std::int16_t>(std::lround(limited * 1000.0));
  }

  /**
   * 周期维持速度命令和失联停车。
   *
   * 有新速度时重复发送以满足 MCU 看门狗；命令超时后发送一次 Stop；完全空闲时
   * 降频发送心跳，兼顾链路检测和 USB 稳定性。
   */
  void MaintainCommand() {
    const auto now = std::chrono::steady_clock::now();
    if (!have_cmd_vel_) {
      /*
       * 空闲心跳只需维持链路诊断，不需要跟随 20 Hz 控制定时器发送。
       * 降低小包频率可避免 CH9102 经 USB Hub 长时间写入时触发 xHCI 端点异常。
       */
      if (now - last_heartbeat_time_ >= idle_heartbeat_interval_) {
        last_control_send_ok_ = client_.SendHeartbeat();
        last_heartbeat_time_ = now;
      }
    } else if (now - last_cmd_vel_time_ > cmd_vel_timeout_) {
      last_control_send_ok_ = client_.SendStop();
      have_cmd_vel_ = false;
      last_heartbeat_time_ = now;
    } else {
      last_control_send_ok_ =
          client_.SendDrive(linear_command_mm_s_, angular_command_mrad_s_);
      if (!last_control_send_ok_) {
        RequestMcuRecovery();
      }
    }
  }

  /** 请求宿主机恢复 USB 映射；冷却时间防止故障时反复重建容器。 */
  void RequestMcuRecovery() {
    /*
     * 容器没有复位宿主机 USB 的权限。这里只写入一个请求文件，由宿主机
     * systemd.path 触发恢复脚本，避免给 ROS2 容器开放 Docker 或 sysfs 权限。
     */
    const auto now = std::chrono::steady_clock::now();
    if (mcu_recovery_request_.empty() ||
        now - last_recovery_request_time_ < recovery_request_cooldown_) {
      return;
    }

    std::ofstream request(mcu_recovery_request_, std::ios::trunc);
    if (!request) {
      RCLCPP_ERROR(get_logger(), "cannot create MCU recovery request: %s",
                   mcu_recovery_request_.c_str());
      return;
    }
    request << "cmd_vel serial write failed\n";
    last_recovery_request_time_ = now;
    RCLCPP_ERROR(get_logger(), "MCU link failed; USB recovery requested");
  }

  /** 解析 JointTrajectory 第一轨迹点，只更新其中明确给出的左右舵机关节。 */
  void OnServoTrajectory(
      const trajectory_msgs::msg::JointTrajectory::SharedPtr message) {
    if (message->points.empty()) {
      return;
    }
    const auto& positions = message->points.front().positions;
    if (positions.size() != message->joint_names.size()) {
      RCLCPP_WARN(get_logger(), "servo trajectory has inconsistent joint data");
      return;
    }

    bool found = false;
    for (std::size_t index = 0; index < message->joint_names.size(); ++index) {
      if (message->joint_names[index] == "left_servo_joint") {
        left_servo_.commanded_rad = positions[index];
        found = true;
      } else if (message->joint_names[index] == "right_servo_joint") {
        right_servo_.commanded_rad = positions[index];
        found = true;
      }
    }
    if (found) {
      client_.SendServo(ServoPulse(left_servo_), ServoPulse(right_servo_));
      if (publish_joint_states_) {
        PublishJointState(now());
      }
    }
  }

  /**
   * 将舵机角度线性映射为 PWM 脉宽。
   * 正负半轴分别使用 center-max 和 min-center，支持不对称机械行程。
   */
  static std::uint16_t ServoPulse(const ServoMapping& mapping) {
    double normalized = 2.0 * mapping.commanded_rad / mapping.range_rad;
    normalized = std::clamp(normalized * mapping.sign, -1.0, 1.0);
    const double pulse = normalized >= 0.0
                             ? mapping.center_us +
                                   normalized * (mapping.max_us - mapping.center_us)
                             : mapping.center_us +
                                   normalized * (mapping.center_us - mapping.min_us);
    return static_cast<std::uint16_t>(std::lround(pulse));
  }

  /** 将空服务调用转换为 MCU 里程计清零命令。 */
  void OnResetOdometry(const std::shared_ptr<std_srvs::srv::Empty::Request>,
                       std::shared_ptr<std_srvs::srv::Empty::Response>) {
    if (!client_.SendOdomReset()) {
      RCLCPP_ERROR(get_logger(), "failed to send odometry reset request");
    }
  }

  /** 高频串口轮询入口；每次轮询后尝试发布所有已经更新的消息类型。 */
  void PollController() {
    client_.Poll();
    PublishOdometry();
    PublishImu();
    PublishRange();
    PublishWheels();
    PublishDiagnostics();
  }

  /** 发布新的 MCU 里程计，并可同步广播 odom -> base_link 动态 TF。 */
  void PublishOdometry() {
    const auto value = client_.GetOdometry();
    if (!value.has_value() || value->mcu_time_ms == last_odom_time_ms_) {
      return;
    }
    last_odom_time_ms_ = value->mcu_time_ms;
    latest_odom_ = value;
    const auto stamp = now();

    nav_msgs::msg::Odometry message;
    message.header.stamp = stamp;
    message.header.frame_id = odom_frame_;
    message.child_frame_id = base_frame_;
    message.pose.pose.position.x = value->x_mm / 1000.0;
    message.pose.pose.position.y = value->y_mm / 1000.0;
    message.pose.pose.position.z = value->z_mm / 1000.0;
    message.pose.pose.orientation = OdomQuaternion(*value);
    message.twist.twist.linear.x = value->speed_mm_s / 1000.0;
    message.twist.twist.angular.z =
        value->yaw_rate_mdeg_s / 1000.0 * kDegreesToRadians;
    SetOdometryCovariance(&message);
    odom_pub_->publish(message);

    if (publish_tf_) {
      geometry_msgs::msg::TransformStamped transform;
      transform.header = message.header;
      transform.child_frame_id = base_frame_;
      transform.transform.translation.x = message.pose.pose.position.x;
      transform.transform.translation.y = message.pose.pose.position.y;
      transform.transform.translation.z = message.pose.pose.position.z;
      transform.transform.rotation = message.pose.pose.orientation;
      tf_broadcaster_->sendTransform(transform);
    }
  }

  /** 把 MCU 的毫度姿态转换为 ROS 四元数。 */
  static geometry_msgs::msg::Quaternion OdomQuaternion(
      const small_car::Odometry& value) {
    return QuaternionFromRpy(value.roll_mdeg / 1000.0 * kDegreesToRadians,
                             value.pitch_mdeg / 1000.0 * kDegreesToRadians,
                             value.yaw_mdeg / 1000.0 * kDegreesToRadians);
  }

  /** 填充 ROS 6x6 位姿/速度协方差；不观测的自由度使用较大方差。 */
  void SetOdometryCovariance(nav_msgs::msg::Odometry* message) const {
    // ROS 协方差是 6x6 行主序矩阵，对角线依次为 x/y/z/roll/pitch/yaw。
    for (const std::size_t index : {0U, 7U, 14U}) {
      message->pose.covariance[index] = odom_position_variance_;
    }
    for (const std::size_t index : {21U, 28U, 35U}) {
      message->pose.covariance[index] = odom_orientation_variance_;
    }
    message->twist.covariance[0] = odom_linear_velocity_variance_;
    message->twist.covariance[7] = 1.0e3;
    message->twist.covariance[14] = 1.0e3;
    message->twist.covariance[21] = 1.0e3;
    message->twist.covariance[28] = 1.0e3;
    message->twist.covariance[35] = odom_angular_velocity_variance_;
  }

  /**
   * 发布新的 IMU 数据。
   *
   * 原始加速度和角速度按当前 ICM20948 量程换算；/imu/data 的姿态取自 MCU
   * 融合里程计，/imu/data_raw 可按配置关闭以避免重复流量。
   */
  void PublishImu() {
    const auto value = client_.GetImuRaw();
    if (!value.has_value() || value->mcu_time_ms == last_imu_time_ms_) {
      return;
    }
    last_imu_time_ms_ = value->mcu_time_ms;
    sensor_msgs::msg::Imu raw;
    raw.header.stamp = now();
    raw.header.frame_id = imu_frame_;
    raw.orientation_covariance[0] = -1.0;
    raw.linear_acceleration.x = value->ax / 2048.0 * kGravity;
    raw.linear_acceleration.y = value->ay / 2048.0 * kGravity;
    raw.linear_acceleration.z = value->az / 2048.0 * kGravity;
    raw.angular_velocity.x = value->gx / 16.4 * kDegreesToRadians;
    raw.angular_velocity.y = value->gy / 16.4 * kDegreesToRadians;
    raw.angular_velocity.z = value->gz / 16.4 * kDegreesToRadians;
    for (const std::size_t index : {0U, 4U, 8U}) {
      raw.linear_acceleration_covariance[index] = imu_acceleration_variance_;
      raw.angular_velocity_covariance[index] = imu_angular_velocity_variance_;
    }
    if (publish_imu_raw_) {
      imu_raw_pub_->publish(raw);
    }

    if (latest_odom_.has_value()) {
      auto fused = raw;
      fused.orientation = OdomQuaternion(latest_odom_.value());
      fused.orientation_covariance[0] = imu_orientation_variance_;
      fused.orientation_covariance[4] = imu_orientation_variance_;
      fused.orientation_covariance[8] = imu_orientation_variance_;
      imu_pub_->publish(fused);
    }
  }

  /** 发布控制源变化和有效超声测距；负距离仅更新控制源，不发布 Range。 */
  void PublishRange() {
    const auto value = client_.GetChassisStatus();
    if (!value.has_value() || value->mcu_time_ms == last_chassis_time_ms_) {
      return;
    }
    last_chassis_time_ms_ = value->mcu_time_ms;
    if (value->source != last_control_source_) {
      std_msgs::msg::UInt8 source;
      source.data = value->source;
      control_source_pub_->publish(source);
      last_control_source_ = value->source;
    }
    if (value->ultra_mm < 0) {
      return;
    }
    sensor_msgs::msg::Range message;
    message.header.stamp = now();
    message.header.frame_id = ultrasonic_frame_;
    message.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
    message.field_of_view = static_cast<float>(ultra_fov_rad_);
    message.min_range = static_cast<float>(ultra_min_m_);
    message.max_range = static_cast<float>(ultra_max_m_);
    message.range = static_cast<float>(value->ultra_mm / 1000.0);
    range_pub_->publish(message);
  }

  /** 对左右轮角速度做时间积分，形成 RViz 可观察的连续轮子转角。 */
  void PublishWheels() {
    if (!publish_joint_states_) {
      return;
    }
    const auto value = client_.GetOdometryDebug();
    if (!value.has_value() || value->mcu_time_ms == last_odom_debug_time_ms_) {
      return;
    }
    if (last_odom_debug_time_ms_ != UINT32_MAX) {
      const std::uint32_t elapsed_ms = value->mcu_time_ms - last_odom_debug_time_ms_;
      if (elapsed_ms < 1000U) {
        const double elapsed_s = elapsed_ms / 1000.0;
        left_wheel_position_rad_ +=
            value->left_speed_mm_s / 1000.0 / wheel_radius_m_ * elapsed_s;
        right_wheel_position_rad_ +=
            value->right_speed_mm_s / 1000.0 / wheel_radius_m_ * elapsed_s;
      }
    }
    last_odom_debug_time_ms_ = value->mcu_time_ms;
    latest_odom_debug_ = value;
    PublishJointState(now());
  }

  /** 合并轮子积分位置、轮速和当前舵机角度，发布完整 JointState。 */
  void PublishJointState(const rclcpp::Time& stamp) {
    sensor_msgs::msg::JointState message;
    message.header.stamp = stamp;
    message.name = {"front_left_wheel_joint", "rear_left_wheel_joint",
                    "front_right_wheel_joint", "rear_right_wheel_joint",
                    "left_servo_joint", "right_servo_joint"};
    message.position = {left_wheel_position_rad_, left_wheel_position_rad_,
                        right_wheel_position_rad_, right_wheel_position_rad_,
                        left_servo_.commanded_rad, right_servo_.commanded_rad};
    message.velocity.assign(message.name.size(), 0.0);
    if (latest_odom_debug_.has_value()) {
      const double left =
          latest_odom_debug_->left_speed_mm_s / 1000.0 / wheel_radius_m_;
      const double right =
          latest_odom_debug_->right_speed_mm_s / 1000.0 / wheel_radius_m_;
      message.velocity[0] = left;
      message.velocity[1] = left;
      message.velocity[2] = right;
      message.velocity[3] = right;
    }
    joint_pub_->publish(message);
  }

  /** 汇总 MCU 外设、主机命令、串口写入和最近 ACK，发布标准诊断消息。 */
  void PublishDiagnostics() {
    const auto value = client_.GetDeviceStatus();
    if (!value.has_value() || value->mcu_time_ms == last_device_time_ms_) {
      return;
    }
    last_device_time_ms_ = value->mcu_time_ms;
    diagnostic_msgs::msg::DiagnosticArray array;
    array.header.stamp = now();
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = "small_car/controller";
    status.hardware_id = "C30D_V2.2_STM32F407";
    status.level = value->error == 0
                       ? diagnostic_msgs::msg::DiagnosticStatus::OK
                       : diagnostic_msgs::msg::DiagnosticStatus::ERROR;
    status.message = value->error == 0 ? "controller ready" : "controller error";
    status.values = {
        DiagnosticValue("gamepad", value->pad_ok ? "connected" : "disconnected"),
        DiagnosticValue("imu", value->imu_ok ? "ready" : "error"),
        DiagnosticValue("ultrasonic", value->ultra_ok ? "ready" : "timeout"),
        DiagnosticValue("error_code", std::to_string(value->error)),
        DiagnosticValue("mcu_time_ms", std::to_string(value->mcu_time_ms)),
        DiagnosticValue("host_linear_mm_s", std::to_string(linear_command_mm_s_)),
        DiagnosticValue("host_angular_mrad_s",
                        std::to_string(angular_command_mrad_s_)),
        DiagnosticValue("serial_write", last_control_send_ok_ ? "ok" : "failed"),
    };

    const auto chassis = client_.GetChassisStatus();
    if (chassis.has_value()) {
      status.values.push_back(
          DiagnosticValue("control_source", std::to_string(chassis->source)));
      status.values.push_back(
          DiagnosticValue("control_enabled", chassis->enabled ? "true" : "false"));
      status.values.push_back(
          DiagnosticValue("control_value_type", std::to_string(chassis->value_type)));
      status.values.push_back(
          DiagnosticValue("mcu_forward_value", std::to_string(chassis->forward)));
      status.values.push_back(
          DiagnosticValue("mcu_turn_value", std::to_string(chassis->turn)));
      status.values.push_back(
          DiagnosticValue("ultrasonic_mm", std::to_string(chassis->ultra_mm)));
    }

    const auto ack = client_.GetLastAck();
    if (ack.has_value()) {
      status.values.push_back(
          DiagnosticValue("last_ack_msg", std::to_string(ack->ack_msg)));
      status.values.push_back(
          DiagnosticValue("last_ack_result", std::to_string(ack->result)));
    }
    array.status.push_back(std::move(status));
    diagnostics_pub_->publish(array);
  }

  // 串口客户端、设备路径和 ROS 坐标系名称。
  small_car::CarClient client_;
  std::string serial_port_;
  std::string chassis_config_;
  std::string cmd_vel_topic_ = "cmd_vel_mcu";
  std::string odom_frame_;
  std::string base_frame_;
  std::string imu_frame_;
  std::string ultrasonic_frame_;
  int baud_rate_ = 115200;

  // ROS SI 单位与 MCU 整数协议之间的限幅、尺寸和测距参数。
  double max_linear_speed_mps_ = 0.6;
  double max_angular_speed_rad_s_ = 2.0;
  double command_rate_hz_ = 20.0;
  double wheel_radius_m_ = 0.0325;
  double ultra_min_m_ = 0.02;
  double ultra_max_m_ = 4.0;
  double ultra_fov_rad_ = 0.52;

  // 发布给定位和融合算法的初始方差；后续应使用实测数据标定。
  double odom_position_variance_ = 0.01;
  double odom_orientation_variance_ = 0.02;
  double odom_linear_velocity_variance_ = 0.04;
  double odom_angular_velocity_variance_ = 0.05;
  double imu_acceleration_variance_ = 0.1;
  double imu_angular_velocity_variance_ = 0.02;
  double imu_orientation_variance_ = 0.02;

  // RViz 轮子动画所需的积分位置，以及可选发布项开关。
  double left_wheel_position_rad_ = 0.0;
  double right_wheel_position_rad_ = 0.0;
  bool publish_tf_ = true;
  bool publish_imu_raw_ = false;
  bool publish_joint_states_ = true;

  // 下行命令超时、空闲心跳和 USB 恢复节流状态。
  std::chrono::milliseconds cmd_vel_timeout_{500};
  std::chrono::milliseconds idle_heartbeat_interval_{1000};
  std::chrono::seconds recovery_request_cooldown_{30};
  std::chrono::steady_clock::time_point last_cmd_vel_time_{};
  std::chrono::steady_clock::time_point last_heartbeat_time_{};
  std::chrono::steady_clock::time_point last_recovery_request_time_{};
  std::string mcu_recovery_request_;
  bool have_cmd_vel_ = false;
  bool last_control_send_ok_ = true;
  std::int16_t linear_command_mm_s_ = 0;
  std::int16_t angular_command_mrad_s_ = 0;

  // 左右舵机当前映射和最近命令角度。
  ServoMapping left_servo_;
  ServoMapping right_servo_;

  // 各 MCU 消息最近时间戳用于去重；UINT*_MAX 表示尚未接收过。
  std::uint32_t last_odom_time_ms_ = UINT32_MAX;
  std::uint32_t last_imu_time_ms_ = UINT32_MAX;
  std::uint32_t last_chassis_time_ms_ = UINT32_MAX;
  std::uint32_t last_odom_debug_time_ms_ = UINT32_MAX;
  std::uint32_t last_device_time_ms_ = UINT32_MAX;
  std::uint8_t last_control_source_ = UINT8_MAX;
  std::optional<small_car::Odometry> latest_odom_;
  std::optional<small_car::OdometryDebug> latest_odom_debug_;

  // ROS 通信对象与定时器，生命周期均由节点统一管理。
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_raw_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_pub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_pub_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_pub_;
  rclcpp::Publisher<std_msgs::msg::UInt8>::SharedPtr control_source_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Subscription<trajectory_msgs::msg::JointTrajectory>::SharedPtr servo_sub_;
  rclcpp::Service<std_srvs::srv::Empty>::SharedPtr reset_odom_service_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  rclcpp::TimerBase::SharedPtr poll_timer_;
  rclcpp::TimerBase::SharedPtr command_timer_;
};

}  // namespace small_car_ros

int main(int argc, char** argv) {
  // 构造或运行异常会被记录为 FATAL，并以非零状态退出供 Docker 重启策略处理。
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<small_car_ros::SmallcarRosAndMcuBridge>());
  } catch (const std::exception& error) {
    RCLCPP_FATAL(rclcpp::get_logger("smallcar_ros_and_mcu_bridge"), "%s",
                 error.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
