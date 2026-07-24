/**
 * @file motion_controller_node.cpp
 * @brief 实现 ROS 2 底盘运动控制、命令仲裁、限速和平滑节点。
 *
 * 节点接收 /cmd_vel 与目标运动请求，输出唯一的 /cmd_vel_mcu，避免多个上层模块
 * 同时直接控制 MCU。安全停止和命令超时在此统一处理。
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

#include <geometry_msgs/msg/twist.hpp>
#include <nav2_msgs/action/drive_on_heading.hpp>
#include <nav2_msgs/action/spin.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/u_int8.hpp>

namespace small_car_motion {
namespace {

constexpr double kPi = 3.14159265358979323846;

/** 将任意弧度角折算到 [-pi, pi]，避免跨越正负 180 度时误判大角度。 */
double NormalizeAngle(double angle) {
  while (angle > kPi) {
    angle -= 2.0 * kPi;
  }
  while (angle < -kPi) {
    angle += 2.0 * kPi;
  }
  return angle;
}

/** 从 ROS 四元数中提取绕 Z 轴的航向角，单位为 rad。 */
double YawFromQuaternion(const geometry_msgs::msg::Quaternion& value) {
  const double sin_yaw = 2.0 * (value.w * value.z + value.x * value.y);
  const double cos_yaw =
      1.0 - 2.0 * (value.y * value.y + value.z * value.z);
  return std::atan2(sin_yaw, cos_yaw);
}

/** 将 ROS Duration 转换为便于控制逻辑使用的秒数。 */
double DurationSeconds(const builtin_interfaces::msg::Duration& duration) {
  return static_cast<double>(duration.sec) +
         static_cast<double>(duration.nanosec) * 1e-9;
}

/** 按单周期最大变化量逼近目标值，用于实现速度和角速度斜坡。 */
double MoveTowards(double current, double target, double maximum_delta) {
  return current + std::clamp(target - current, -maximum_delta, maximum_delta);
}

}  // namespace

/**
 * 底盘运动控制节点。
 *
 * 命令优先关系为：最新直接速度命令会取消 Action；手柄接管时也会取消 Action。
 * 所有来源最终只通过 cmd_vel_mcu 向桥接节点输出一条平滑后的速度命令。
 */
class MotionController : public rclcpp::Node {
 public:
  using Drive = nav2_msgs::action::DriveOnHeading;
  using DriveHandle = rclcpp_action::ServerGoalHandle<Drive>;
  using Spin = nav2_msgs::action::Spin;
  using SpinHandle = rclcpp_action::ServerGoalHandle<Spin>;

  /** 读取配置，创建话题/Action 接口，并启动固定频率控制定时器。 */
  MotionController() : Node("small_car_motion_controller") {
    DeclareParameters();
    ReadParameters();

    output_pub_ =
        create_publisher<geometry_msgs::msg::Twist>("cmd_vel_mcu", 10);
    direct_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "cmd_vel", 10,
        std::bind(&MotionController::OnDirectCommand, this,
                  std::placeholders::_1));
    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
        "odom", 20,
        std::bind(&MotionController::OnOdometry, this,
                  std::placeholders::_1));
    control_source_sub_ = create_subscription<std_msgs::msg::UInt8>(
        "control/source", 10,
        std::bind(&MotionController::OnControlSource, this,
                  std::placeholders::_1));
    range_sub_ = create_subscription<sensor_msgs::msg::Range>(
        "ultrasonic/front", rclcpp::SensorDataQoS(),
        std::bind(&MotionController::OnFrontRange, this,
                  std::placeholders::_1));

    drive_server_ = rclcpp_action::create_server<Drive>(
        this, "drive_on_heading",
        std::bind(&MotionController::OnDriveGoal, this,
                  std::placeholders::_1, std::placeholders::_2),
        std::bind(&MotionController::OnDriveCancel, this,
                  std::placeholders::_1),
        std::bind(&MotionController::OnDriveAccepted, this,
                  std::placeholders::_1));
    spin_server_ = rclcpp_action::create_server<Spin>(
        this, "spin",
        std::bind(&MotionController::OnSpinGoal, this,
                  std::placeholders::_1, std::placeholders::_2),
        std::bind(&MotionController::OnSpinCancel, this,
                  std::placeholders::_1),
        std::bind(&MotionController::OnSpinAccepted, this,
                  std::placeholders::_1));

    const auto period = std::chrono::duration<double>(1.0 / control_rate_hz_);
    timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&MotionController::OnControlTimer, this));
    last_timer_time_ = std::chrono::steady_clock::now();
    RCLCPP_INFO(get_logger(),
                "motion controller ready: /cmd_vel -> /cmd_vel_mcu");
  }

 private:
  /** 当前控制模式；任意时刻只允许一个模式生成目标速度。 */
  enum class Mode {
    kDirect,
    kDrive,
    kSpin,
  };

  /** 用于定距和定角控制的平面位姿快照，单位分别为 m 和 rad。 */
  struct Pose2d {
    double x = 0.0;
    double y = 0.0;
    double yaw = 0.0;
  };

  /** 声明所有可由 motion_controller.yaml 覆盖的控制参数。 */
  void DeclareParameters() {
    declare_parameter<double>("control_rate_hz", 20.0);
    declare_parameter<double>("direct_command_timeout_s", 0.5);
    declare_parameter<double>("odom_timeout_s", 0.4);
    declare_parameter<double>("default_action_timeout_s", 15.0);
    declare_parameter<double>("front_obstacle_stop_m", 0.2);
    declare_parameter<double>("drive_kp", 1.2);
    declare_parameter<double>("drive_min_speed_mps", 0.12);
    declare_parameter<double>("drive_max_speed_mps", 0.35);
    declare_parameter<double>("drive_tolerance_m", 0.02);
    declare_parameter<double>("heading_kp", 1.5);
    declare_parameter<double>("heading_max_angular_rad_s", 0.6);
    declare_parameter<double>("spin_kp", 2.0);
    declare_parameter<double>("spin_min_speed_rad_s", 0.45);
    declare_parameter<double>("spin_max_speed_rad_s", 1.2);
    declare_parameter<double>("spin_tolerance_rad", 0.035);
    declare_parameter<double>("linear_accel_limit_mps2", 0.5);
    declare_parameter<double>("angular_accel_limit_rad_s2", 1.8);
    declare_parameter<int>("settle_cycles", 3);
  }

  /** 启动时一次性读取参数，运行期间不会自动重新加载 YAML。 */
  void ReadParameters() {
    control_rate_hz_ = get_parameter("control_rate_hz").as_double();
    direct_timeout_s_ =
        get_parameter("direct_command_timeout_s").as_double();
    odom_timeout_s_ = get_parameter("odom_timeout_s").as_double();
    default_action_timeout_s_ =
        get_parameter("default_action_timeout_s").as_double();
    front_obstacle_stop_m_ =
        get_parameter("front_obstacle_stop_m").as_double();
    drive_kp_ = get_parameter("drive_kp").as_double();
    drive_min_speed_ = get_parameter("drive_min_speed_mps").as_double();
    drive_max_speed_ = get_parameter("drive_max_speed_mps").as_double();
    drive_tolerance_ = get_parameter("drive_tolerance_m").as_double();
    heading_kp_ = get_parameter("heading_kp").as_double();
    heading_max_angular_ =
        get_parameter("heading_max_angular_rad_s").as_double();
    spin_kp_ = get_parameter("spin_kp").as_double();
    spin_min_speed_ = get_parameter("spin_min_speed_rad_s").as_double();
    spin_max_speed_ = get_parameter("spin_max_speed_rad_s").as_double();
    spin_tolerance_ = get_parameter("spin_tolerance_rad").as_double();
    linear_accel_limit_ =
        get_parameter("linear_accel_limit_mps2").as_double();
    angular_accel_limit_ =
        get_parameter("angular_accel_limit_rad_s2").as_double();
    settle_cycles_required_ =
        static_cast<int>(get_parameter("settle_cycles").as_int());
  }

  /** 更新最新平面位姿，并在旋转模式下累计跨越 ±pi 的实际转角。 */
  void OnOdometry(const nav_msgs::msg::Odometry::SharedPtr message) {
    const double yaw = YawFromQuaternion(message->pose.pose.orientation);
    if (have_odom_ && mode_ == Mode::kSpin) {
      spin_traveled_ += NormalizeAngle(yaw - pose_.yaw);
    }
    pose_.x = message->pose.pose.position.x;
    pose_.y = message->pose.pose.position.y;
    pose_.yaw = yaw;
    have_odom_ = true;
    last_odom_time_ = std::chrono::steady_clock::now();
  }

  /**
   * 接收人工、导航或语音发布的直接速度。
   * 小车只支持平面差速运动，因此主动清零其余四个自由度。
   */
  void OnDirectCommand(const geometry_msgs::msg::Twist::SharedPtr message) {
    if (mode_ != Mode::kDirect) {
      AbortActive("被直接速度命令接管");
    }
    direct_command_ = *message;
    direct_command_.linear.y = 0.0;
    direct_command_.linear.z = 0.0;
    direct_command_.angular.x = 0.0;
    direct_command_.angular.y = 0.0;
    last_direct_time_ = std::chrono::steady_clock::now();
    have_direct_command_ = true;

    /* 显式零速度属于停车命令，不经过斜坡，保证人工控制能立即停止。 */
    if (message->linear.x == 0.0 && message->angular.z == 0.0) {
      ResetOutput();
    }
  }

  /** 手柄成为控制源时终止自动动作，保证本地人工控制优先。 */
  void OnControlSource(const std_msgs::msg::UInt8::SharedPtr message) {
    constexpr std::uint8_t kGamepadSource = 2;
    if ((message->data == kGamepadSource) && (mode_ != Mode::kDirect)) {
      AbortActive("手柄已接管");
    }
  }

  /** 更新前方障碍标志；无效或超量程数据不会触发停车。 */
  void OnFrontRange(const sensor_msgs::msg::Range::SharedPtr message) {
    front_obstacle_near_ =
        std::isfinite(message->range) &&
        message->range >= message->min_range &&
        message->range <= front_obstacle_stop_m_;
  }

  /** 检查定距目标是否具有有效里程计、非零距离和前向目标。 */
  rclcpp_action::GoalResponse OnDriveGoal(
      const rclcpp_action::GoalUUID&,
      std::shared_ptr<const Drive::Goal> goal) {
    const double distance =
        std::hypot(goal->target.x, goal->target.y);
    if (!have_odom_ || distance <= 0.0 || goal->speed <= 0.0 ||
        std::abs(goal->target.y) > 1e-6) {
      return rclcpp_action::GoalResponse::REJECT;
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  /** Action 取消请求总是接受，实际停车在下一控制周期完成。 */
  rclcpp_action::CancelResponse OnDriveCancel(
      const std::shared_ptr<DriveHandle>) {
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  /** 保存定距动作的起点、方向、限速和截止时间。 */
  void OnDriveAccepted(const std::shared_ptr<DriveHandle> handle) {
    AbortActive("被新的定距目标替代");
    drive_handle_ = handle;
    mode_ = Mode::kDrive;
    action_start_pose_ = pose_;
    drive_target_m_ =
        std::copysign(std::hypot(handle->get_goal()->target.x,
                                handle->get_goal()->target.y),
                      handle->get_goal()->target.x);
    drive_speed_limit_ =
        std::clamp(static_cast<double>(handle->get_goal()->speed),
                   drive_min_speed_, drive_max_speed_);
    StartActionTimeout(DurationSeconds(handle->get_goal()->time_allowance));
    settle_cycles_ = 0;
    ResetOutput();
    RCLCPP_INFO(get_logger(), "drive goal accepted: %.3f m",
                drive_target_m_);
  }

  /** 检查定角目标是否具备有效里程计且目标角度非零。 */
  rclcpp_action::GoalResponse OnSpinGoal(
      const rclcpp_action::GoalUUID&,
      std::shared_ptr<const Spin::Goal> goal) {
    if (!have_odom_ || std::abs(goal->target_yaw) <= 0.0) {
      return rclcpp_action::GoalResponse::REJECT;
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse OnSpinCancel(
      const std::shared_ptr<SpinHandle>) {
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  /** 保存定角动作目标，并从零开始累计实际旋转角。 */
  void OnSpinAccepted(const std::shared_ptr<SpinHandle> handle) {
    AbortActive("被新的定角目标替代");
    spin_handle_ = handle;
    mode_ = Mode::kSpin;
    spin_target_rad_ = handle->get_goal()->target_yaw;
    spin_traveled_ = 0.0;
    StartActionTimeout(DurationSeconds(handle->get_goal()->time_allowance));
    settle_cycles_ = 0;
    ResetOutput();
    RCLCPP_INFO(get_logger(), "spin goal accepted: %.3f rad",
                spin_target_rad_);
  }

  /** 使用 Action 指定时限；未指定时采用配置中的默认超时。 */
  void StartActionTimeout(double requested_s) {
    const double timeout =
        requested_s > 0.0 ? requested_s : default_action_timeout_s_;
    action_deadline_ =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(timeout));
  }

  /** 每周期检查里程计新鲜度和动作总时限，失败时立即终止动作。 */
  bool ActionStateValid() {
    const auto now = std::chrono::steady_clock::now();
    if (!have_odom_ ||
        std::chrono::duration<double>(now - last_odom_time_).count() >
            odom_timeout_s_) {
      AbortActive("里程计超时");
      return false;
    }
    if (now > action_deadline_) {
      AbortActive("动作超时");
      return false;
    }
    return true;
  }

  /**
   * 更新定距动作。
   *
   * 位移投影到动作起始航向，得到带符号的直线行程；比例控制决定线速度，
   * 同时使用航向误差产生小角速度修正。连续多个周期进入容差才判定完成。
   */
  void UpdateDrive() {
    if (!drive_handle_ || !ActionStateValid()) {
      return;
    }
    if ((drive_target_m_ > 0.0) && front_obstacle_near_) {
      AbortActive("前方障碍物过近");
      return;
    }
    if (drive_handle_->is_canceling()) {
      auto result = std::make_shared<Drive::Result>();
      result->error_code = 0;
      result->error_msg = "动作已取消";
      drive_handle_->canceled(result);
      FinishAction();
      return;
    }

    const double dx = pose_.x - action_start_pose_.x;
    const double dy = pose_.y - action_start_pose_.y;
    const double traveled =
        dx * std::cos(action_start_pose_.yaw) +
        dy * std::sin(action_start_pose_.yaw);
    const double remaining = drive_target_m_ - traveled;

    auto feedback = std::make_shared<Drive::Feedback>();
    feedback->distance_traveled = static_cast<float>(std::abs(traveled));
    drive_handle_->publish_feedback(feedback);

    if (std::abs(remaining) <= drive_tolerance_) {
      if (++settle_cycles_ >= settle_cycles_required_) {
        auto result = std::make_shared<Drive::Result>();
        result->error_code = 0;
        result->error_msg = "";
        drive_handle_->succeed(result);
        FinishAction();
      }
      return;
    }
    settle_cycles_ = 0;

    const double speed =
        std::clamp(drive_kp_ * std::abs(remaining),
                   drive_min_speed_, drive_speed_limit_);
    target_output_.linear.x = std::copysign(speed, remaining);
    const double heading_error =
        NormalizeAngle(action_start_pose_.yaw - pose_.yaw);
    target_output_.angular.z =
        std::clamp(heading_kp_ * heading_error,
                   -heading_max_angular_, heading_max_angular_);
  }

  /**
   * 更新定角动作。
   *
   * 使用累计转角而不是首尾航向差，因而能够正确处理跨越 ±pi 的旋转。
   */
  void UpdateSpin() {
    if (!spin_handle_ || !ActionStateValid()) {
      return;
    }
    if (spin_handle_->is_canceling()) {
      auto result = std::make_shared<Spin::Result>();
      result->error_code = 0;
      result->error_msg = "动作已取消";
      spin_handle_->canceled(result);
      FinishAction();
      return;
    }

    const double remaining = spin_target_rad_ - spin_traveled_;
    auto feedback = std::make_shared<Spin::Feedback>();
    feedback->angular_distance_traveled =
        static_cast<float>(spin_traveled_);
    spin_handle_->publish_feedback(feedback);

    if (std::abs(remaining) <= spin_tolerance_) {
      if (++settle_cycles_ >= settle_cycles_required_) {
        auto result = std::make_shared<Spin::Result>();
        result->error_code = 0;
        result->error_msg = "";
        spin_handle_->succeed(result);
        FinishAction();
      }
      return;
    }
    settle_cycles_ = 0;

    const double speed =
        std::clamp(spin_kp_ * std::abs(remaining),
                   spin_min_speed_, spin_max_speed_);
    target_output_.linear.x = 0.0;
    target_output_.angular.z = std::copysign(speed, remaining);
  }

  /** 以失败结果结束仍活动的 Action，清除状态并立即发布零速度。 */
  void AbortActive(const std::string& reason) {
    if (drive_handle_ && drive_handle_->is_active()) {
      auto result = std::make_shared<Drive::Result>();
      result->error_code = 1;
      result->error_msg = reason;
      drive_handle_->abort(result);
    }
    if (spin_handle_ && spin_handle_->is_active()) {
      auto result = std::make_shared<Spin::Result>();
      result->error_code = 1;
      result->error_msg = reason;
      spin_handle_->abort(result);
    }
    if (mode_ != Mode::kDirect) {
      RCLCPP_WARN(get_logger(), "motion action stopped: %s", reason.c_str());
    }
    drive_handle_.reset();
    spin_handle_.reset();
    mode_ = Mode::kDirect;
    ResetOutput();
  }

  /** 在 Action 成功或取消后回到直接速度模式，并阻止旧命令恢复。 */
  void FinishAction() {
    drive_handle_.reset();
    spin_handle_.reset();
    mode_ = Mode::kDirect;
    have_direct_command_ = false;
    ResetOutput();
  }

  /** 同时清零目标值和斜坡输出，并立刻发布停车命令。 */
  void ResetOutput() {
    target_output_ = geometry_msgs::msg::Twist();
    current_output_ = geometry_msgs::msg::Twist();
    output_pub_->publish(current_output_);
  }

  /**
   * 固定频率控制主循环。
   *
   * 先根据模式计算目标速度，再应用加速度限制生成当前输出。直接命令超时后
   * 自动归零，避免发布者退出或网络中断时保持最后速度。
   */
  void OnControlTimer() {
    const auto now = std::chrono::steady_clock::now();
    const double dt =
        std::chrono::duration<double>(now - last_timer_time_).count();
    last_timer_time_ = now;
    target_output_ = geometry_msgs::msg::Twist();

    if (mode_ == Mode::kDrive) {
      UpdateDrive();
    } else if (mode_ == Mode::kSpin) {
      UpdateSpin();
    } else if (have_direct_command_ &&
               std::chrono::duration<double>(now - last_direct_time_).count() <=
                   direct_timeout_s_) {
      target_output_ = direct_command_;
    } else {
      if (have_direct_command_) {
        ResetOutput();
      }
      have_direct_command_ = false;
    }

    current_output_.linear.x =
        MoveTowards(current_output_.linear.x, target_output_.linear.x,
                    linear_accel_limit_ * dt);
    current_output_.angular.z =
        MoveTowards(current_output_.angular.z, target_output_.angular.z,
                    angular_accel_limit_ * dt);
    output_pub_->publish(current_output_);
  }

  // 控制周期和各输入的失效保护时间。
  double control_rate_hz_ = 20.0;
  double direct_timeout_s_ = 0.5;
  double odom_timeout_s_ = 0.4;
  double default_action_timeout_s_ = 15.0;

  // 定距控制、航向保持和前向避障参数。
  double front_obstacle_stop_m_ = 0.2;
  double drive_kp_ = 1.2;
  double drive_min_speed_ = 0.12;
  double drive_max_speed_ = 0.35;
  double drive_tolerance_ = 0.02;
  double heading_kp_ = 1.5;
  double heading_max_angular_ = 0.6;

  // 定角控制和输出斜坡参数。
  double spin_kp_ = 2.0;
  double spin_min_speed_ = 0.45;
  double spin_max_speed_ = 1.2;
  double spin_tolerance_ = 0.035;
  double linear_accel_limit_ = 0.5;
  double angular_accel_limit_ = 1.8;
  int settle_cycles_required_ = 3;
  int settle_cycles_ = 0;

  // 当前动作状态及其起点、目标和传感器有效标志。
  Mode mode_ = Mode::kDirect;
  Pose2d pose_;
  Pose2d action_start_pose_;
  bool have_odom_ = false;
  bool have_direct_command_ = false;
  bool front_obstacle_near_ = false;
  double drive_target_m_ = 0.0;
  double drive_speed_limit_ = 0.0;
  double spin_target_rad_ = 0.0;
  double spin_traveled_ = 0.0;

  // direct_command_ 是最近输入，target_output_ 是本周期目标，
  // current_output_ 是经过加速度限制后真正发布给 MCU 的值。
  geometry_msgs::msg::Twist direct_command_;
  geometry_msgs::msg::Twist target_output_;
  geometry_msgs::msg::Twist current_output_;
  std::chrono::steady_clock::time_point last_odom_time_;
  std::chrono::steady_clock::time_point last_direct_time_;
  std::chrono::steady_clock::time_point last_timer_time_;
  std::chrono::steady_clock::time_point action_deadline_;

  // ROS 通信对象由节点持有，生命周期与 MotionController 一致。
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr output_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr direct_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr control_source_sub_;
  rclcpp::Subscription<sensor_msgs::msg::Range>::SharedPtr range_sub_;
  rclcpp_action::Server<Drive>::SharedPtr drive_server_;
  rclcpp_action::Server<Spin>::SharedPtr spin_server_;
  std::shared_ptr<DriveHandle> drive_handle_;
  std::shared_ptr<SpinHandle> spin_handle_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace small_car_motion

int main(int argc, char** argv) {
  // 使用单线程执行器即可：全部回调只访问本节点状态，不需要并行执行。
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<small_car_motion::MotionController>());
  rclcpp::shutdown();
  return 0;
}
