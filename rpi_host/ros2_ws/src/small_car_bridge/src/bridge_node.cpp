#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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

diagnostic_msgs::msg::KeyValue DiagnosticValue(const std::string& key,
                                                const std::string& value) {
  diagnostic_msgs::msg::KeyValue result;
  result.key = key;
  result.value = value;
  return result;
}

}  // namespace

class SmallCarBridge : public rclcpp::Node {
 public:
  SmallCarBridge() : Node("small_car_bridge") {
    DeclareParameters();
    ReadParameters();
    OpenController();
    CreateRosInterfaces();
    RCLCPP_INFO(get_logger(), "ROS2 bridge ready: %s @ %d", serial_port_.c_str(),
                baud_rate_);
  }

  ~SmallCarBridge() override {
    if (client_.IsOpen()) {
      client_.SendStop();
      client_.Close();
    }
  }

 private:
  struct ServoMapping {
    int center_us = 1500;
    int min_us = 800;
    int max_us = 2200;
    double range_rad = kPi;
    double sign = 1.0;
    double commanded_rad = 0.0;
  };

  void DeclareParameters() {
    declare_parameter<std::string>("serial_port", "/dev/ttyACM0");
    declare_parameter<int>("baud_rate", 115200);
    declare_parameter<std::string>("chassis_config", "");
    declare_parameter<double>("max_linear_speed_mps", 0.6);
    declare_parameter<double>("max_angular_speed_rad_s", 2.0);
    declare_parameter<int>("cmd_vel_timeout_ms", 500);
    declare_parameter<double>("command_rate_hz", 20.0);
    declare_parameter<double>("turn_sign", 1.0);
    declare_parameter<std::string>("odom_frame", "odom");
    declare_parameter<std::string>("base_frame", "base_link");
    declare_parameter<std::string>("imu_frame", "imu_link");
    declare_parameter<std::string>("ultrasonic_frame", "ultrasonic_link");
    declare_parameter<bool>("publish_tf", true);
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

  void DeclareServoParameters(const std::string& name, int center, int minimum,
                              int maximum) {
    declare_parameter<int>(name + "_servo_center_us", center);
    declare_parameter<int>(name + "_servo_min_us", minimum);
    declare_parameter<int>(name + "_servo_max_us", maximum);
    declare_parameter<double>(name + "_servo_range_rad", kPi);
    declare_parameter<double>(name + "_servo_sign", 1.0);
  }

  void ReadParameters() {
    serial_port_ = get_parameter("serial_port").as_string();
    baud_rate_ = static_cast<int>(get_parameter("baud_rate").as_int());
    chassis_config_ = get_parameter("chassis_config").as_string();
    if (chassis_config_.empty()) {
      chassis_config_ = ament_index_cpp::get_package_share_directory("small_car_bridge") +
                        "/config/chassis_params.yaml";
    }
    max_linear_speed_mps_ = get_parameter("max_linear_speed_mps").as_double();
    max_angular_speed_rad_s_ = get_parameter("max_angular_speed_rad_s").as_double();
    cmd_vel_timeout_ =
        std::chrono::milliseconds(get_parameter("cmd_vel_timeout_ms").as_int());
    command_rate_hz_ = get_parameter("command_rate_hz").as_double();
    turn_sign_ = get_parameter("turn_sign").as_double();
    odom_frame_ = get_parameter("odom_frame").as_string();
    base_frame_ = get_parameter("base_frame").as_string();
    imu_frame_ = get_parameter("imu_frame").as_string();
    ultrasonic_frame_ = get_parameter("ultrasonic_frame").as_string();
    publish_tf_ = get_parameter("publish_tf").as_bool();
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

  void OpenController() {
    if (!client_.Open(serial_port_, baud_rate_)) {
      throw std::runtime_error("cannot open serial port: " + serial_port_);
    }
    const auto parameters = small_car::LoadChassisConfig(chassis_config_);
    std::string error;
    if (!small_car::ApplyChassisConfig(&client_, parameters,
                                       std::chrono::milliseconds(500), &error)) {
      client_.Close();
      throw std::runtime_error("cannot apply chassis config: " + error);
    }
    RCLCPP_INFO(get_logger(), "applied and verified %zu chassis parameters",
                parameters.size());
  }

  void CreateRosInterfaces() {
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom", 10);
    imu_raw_pub_ = create_publisher<sensor_msgs::msg::Imu>(
        "imu/data_raw", rclcpp::SensorDataQoS());
    imu_pub_ = create_publisher<sensor_msgs::msg::Imu>(
        "imu/data", rclcpp::SensorDataQoS());
    range_pub_ = create_publisher<sensor_msgs::msg::Range>(
        "ultrasonic/front", rclcpp::SensorDataQoS());
    joint_pub_ = create_publisher<sensor_msgs::msg::JointState>("joint_states", 10);
    diagnostics_pub_ =
        create_publisher<diagnostic_msgs::msg::DiagnosticArray>("diagnostics", 10);
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "cmd_vel", 10,
        std::bind(&SmallCarBridge::OnCmdVel, this, std::placeholders::_1));
    servo_sub_ = create_subscription<trajectory_msgs::msg::JointTrajectory>(
        "servo_controller/joint_trajectory", 10,
        std::bind(&SmallCarBridge::OnServoTrajectory, this, std::placeholders::_1));
    reset_odom_service_ = create_service<std_srvs::srv::Empty>(
        "reset_odometry",
        std::bind(&SmallCarBridge::OnResetOdometry, this, std::placeholders::_1,
                  std::placeholders::_2));

    poll_timer_ = create_wall_timer(std::chrono::milliseconds(5),
                                    std::bind(&SmallCarBridge::PollController, this));
    const auto period = std::chrono::duration<double>(1.0 / command_rate_hz_);
    command_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&SmallCarBridge::MaintainCommand, this));
  }

  void OnCmdVel(const geometry_msgs::msg::Twist::SharedPtr message) {
    forward_command_ = ScaleCommand(message->linear.x, max_linear_speed_mps_);
    turn_command_ =
        ScaleCommand(message->angular.z * turn_sign_, max_angular_speed_rad_s_);
    last_cmd_vel_time_ = std::chrono::steady_clock::now();
    have_cmd_vel_ = true;
    client_.SendDrive(forward_command_, turn_command_);
  }

  static std::int16_t ScaleCommand(double value, double maximum) {
    return static_cast<std::int16_t>(
        std::lround(std::clamp(value / maximum, -1.0, 1.0) * 1000.0));
  }

  void MaintainCommand() {
    if (!have_cmd_vel_) {
      client_.SendHeartbeat();
    } else if (std::chrono::steady_clock::now() - last_cmd_vel_time_ >
               cmd_vel_timeout_) {
      client_.SendStop();
      have_cmd_vel_ = false;
    } else {
      client_.SendDrive(forward_command_, turn_command_);
    }
  }

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
      PublishJointState(now());
    }
  }

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

  void OnResetOdometry(const std::shared_ptr<std_srvs::srv::Empty::Request>,
                       std::shared_ptr<std_srvs::srv::Empty::Response>) {
    if (!client_.SendOdomReset()) {
      RCLCPP_ERROR(get_logger(), "failed to send odometry reset request");
    }
  }

  void PollController() {
    client_.Poll();
    PublishOdometry();
    PublishImu();
    PublishRange();
    PublishWheels();
    PublishDiagnostics();
  }

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

  static geometry_msgs::msg::Quaternion OdomQuaternion(
      const small_car::Odometry& value) {
    return QuaternionFromRpy(value.roll_mdeg / 1000.0 * kDegreesToRadians,
                             value.pitch_mdeg / 1000.0 * kDegreesToRadians,
                             value.yaw_mdeg / 1000.0 * kDegreesToRadians);
  }

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
    imu_raw_pub_->publish(raw);

    if (latest_odom_.has_value()) {
      auto fused = raw;
      fused.orientation = OdomQuaternion(latest_odom_.value());
      fused.orientation_covariance[0] = imu_orientation_variance_;
      fused.orientation_covariance[4] = imu_orientation_variance_;
      fused.orientation_covariance[8] = imu_orientation_variance_;
      imu_pub_->publish(fused);
    }
  }

  void PublishRange() {
    const auto value = client_.GetChassisStatus();
    if (!value.has_value() || value->mcu_time_ms == last_chassis_time_ms_) {
      return;
    }
    last_chassis_time_ms_ = value->mcu_time_ms;
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

  void PublishWheels() {
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
    };
    array.status.push_back(std::move(status));
    diagnostics_pub_->publish(array);
  }

  small_car::CarClient client_;
  std::string serial_port_;
  std::string chassis_config_;
  std::string odom_frame_;
  std::string base_frame_;
  std::string imu_frame_;
  std::string ultrasonic_frame_;
  int baud_rate_ = 115200;
  double max_linear_speed_mps_ = 0.6;
  double max_angular_speed_rad_s_ = 2.0;
  double command_rate_hz_ = 20.0;
  double turn_sign_ = 1.0;
  double wheel_radius_m_ = 0.0325;
  double ultra_min_m_ = 0.02;
  double ultra_max_m_ = 4.0;
  double ultra_fov_rad_ = 0.52;
  double odom_position_variance_ = 0.01;
  double odom_orientation_variance_ = 0.02;
  double odom_linear_velocity_variance_ = 0.04;
  double odom_angular_velocity_variance_ = 0.05;
  double imu_acceleration_variance_ = 0.1;
  double imu_angular_velocity_variance_ = 0.02;
  double imu_orientation_variance_ = 0.02;
  double left_wheel_position_rad_ = 0.0;
  double right_wheel_position_rad_ = 0.0;
  bool publish_tf_ = true;
  std::chrono::milliseconds cmd_vel_timeout_{500};
  std::chrono::steady_clock::time_point last_cmd_vel_time_{};
  bool have_cmd_vel_ = false;
  std::int16_t forward_command_ = 0;
  std::int16_t turn_command_ = 0;
  ServoMapping left_servo_;
  ServoMapping right_servo_;

  std::uint32_t last_odom_time_ms_ = UINT32_MAX;
  std::uint32_t last_imu_time_ms_ = UINT32_MAX;
  std::uint32_t last_chassis_time_ms_ = UINT32_MAX;
  std::uint32_t last_odom_debug_time_ms_ = UINT32_MAX;
  std::uint32_t last_device_time_ms_ = UINT32_MAX;
  std::optional<small_car::Odometry> latest_odom_;
  std::optional<small_car::OdometryDebug> latest_odom_debug_;

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_raw_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_pub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_pub_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Subscription<trajectory_msgs::msg::JointTrajectory>::SharedPtr servo_sub_;
  rclcpp::Service<std_srvs::srv::Empty>::SharedPtr reset_odom_service_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  rclcpp::TimerBase::SharedPtr poll_timer_;
  rclcpp::TimerBase::SharedPtr command_timer_;
};

}  // namespace small_car_ros

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<small_car_ros::SmallCarBridge>());
  } catch (const std::exception& error) {
    RCLCPP_FATAL(rclcpp::get_logger("small_car_bridge"), "%s", error.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
