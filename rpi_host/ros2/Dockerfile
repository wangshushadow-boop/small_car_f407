FROM ros:kilted-ros-base

RUN apt-get update && apt-get install -y --no-install-recommends \
      libyaml-cpp-dev \
      alsa-utils \
      libportaudio2 \
      mpg123 \
      python3-numpy \
      python3-colcon-common-extensions \
      python3-pip \
      ros-kilted-ament-index-python \
      ros-kilted-diagnostic-msgs \
      ros-kilted-nav-msgs \
      ros-kilted-navigation2 \
      ros-kilted-nav2-bringup \
      ros-kilted-robot-state-publisher \
      ros-kilted-sensor-msgs \
      ros-kilted-std-msgs \
      ros-kilted-std-srvs \
      ros-kilted-tf2-ros \
      ros-kilted-trajectory-msgs \
      ros-kilted-xacro \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages --no-cache-dir \
      --index-url https://pypi.tuna.tsinghua.edu.cn/simple \
      --timeout 120 --retries 10 sounddevice

WORKDIR /workspace/rpi_host
