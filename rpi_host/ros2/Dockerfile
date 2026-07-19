FROM ros:kilted-ros-base

RUN apt-get update && apt-get install -y --no-install-recommends \
      libyaml-cpp-dev \
      python3-colcon-common-extensions \
      ros-kilted-ament-index-python \
      ros-kilted-diagnostic-msgs \
      ros-kilted-nav-msgs \
      ros-kilted-robot-state-publisher \
      ros-kilted-sensor-msgs \
      ros-kilted-std-srvs \
      ros-kilted-tf2-ros \
      ros-kilted-trajectory-msgs \
      ros-kilted-xacro \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/rpi_host/ros2_ws
