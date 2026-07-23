param(
  # 树莓派的 IP 地址。默认是当前项目调试用的固定地址。
  [string]$HostAddress = "192.168.3.85",

  # 树莓派登录用户名。
  [string]$UserName = "ubuntu",

  # 树莓派上的 rpi_host 目标目录。
  # 注意：这里指向 rpi_host 本身，不是仓库根目录。
  [string]$RemoteProject = "/home/ubuntu/small_car_f407/rpi_host",

  # SSH 端口，默认 22。
  [int]$SshPort = 22
)

# 任意一步失败时立即停止脚本，避免继续执行后续构建或启动旧程序。
$ErrorActionPreference = "Stop"

# 当前脚本位于 scripts/ 目录，父目录就是仓库根目录。
$project_root = Split-Path -Parent $PSScriptRoot

# 本地临时压缩包路径。使用 GUID 避免多次运行时文件名冲突。
# 这里拆成多行写，避免不同 PowerShell 版本对 -f (...) 的解析差异。
$archive_id = [guid]::NewGuid()
$archive_name = "small_car_rpi_host_" + $archive_id.ToString("N") + ".tar.gz"
$archive_path = Join-Path -Path $env:TEMP -ChildPath $archive_name

# 上传到树莓派 /tmp 下的临时压缩包路径。
$remote_archive = "/tmp/small_car_rpi_host.tar.gz"

# scp/ssh 使用的远程登录目标，例如 ubuntu@192.168.3.85。
$remote_target = "${UserName}@${HostAddress}"

try {
  Write-Host "[1/4] Packing rpi_host sources..."
  Push-Location $project_root
  try {
    # 只打包 rpi_host 源码和配置，不打包本地/远端生成文件。
    #
    # 排除 build、build-*：
    #   这些是 CMake 生成目录，Windows 和树莓派架构不同，不能混用。
    #
    # 排除 ros2_ws/build、install、log：
    #   这些由 colcon / Docker 生成，体积大，且有些文件可能属于 root。
    #
    # 排除 jpg、wav：
    #   这些通常是相机、音频测试产物，不应该跟源码一起同步。
    $tar_args = @("-czf", $archive_path)
    $tar_args += "--exclude=rpi_host/build"
    $tar_args += "--exclude=rpi_host/build-*"
    $tar_args += "--exclude=rpi_host/ros2_ws/build"
    $tar_args += "--exclude=rpi_host/ros2_ws/install"
    $tar_args += "--exclude=rpi_host/ros2_ws/log"
    $tar_args += "--exclude=rpi_host/.cache"
    $tar_args += "--exclude=rpi_host/*.jpg"
    $tar_args += "--exclude=rpi_host/*.wav"
    $tar_args += "rpi_host"
    & tar @tar_args
    if ($LASTEXITCODE -ne 0) {
      throw "Command failed: tar"
    }
  } finally {
    Pop-Location
  }

  Write-Host "[2/4] Uploading to ${remote_target}..."
  # 把本地压缩包上传到树莓派 /tmp。
  # 后续在树莓派本机解压，速度比逐文件 scp 更稳定。
  $scp_args = @("-P", $SshPort.ToString(), $archive_path, "${remote_target}:${remote_archive}")
  & scp @scp_args
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed: scp"
  }

  Write-Host "[3/4] Building host tools on Raspberry Pi..."

  # 远端执行流程：
  # 1. 如果 ROS2 bridge 正在运行，先 docker compose down，释放串口和工作区文件。
  # 2. 创建目标目录。
  # 3. 只删除源码目录和普通文档文件，然后解压新的源码。
  # 4. 重建 rpi_host C++ 工具并运行协议测试。
  # 5. 重建并启动 ROS2 bridge 容器。
  #
  # 这里没有使用 rm -rf $RemoteProject/* 清空整个目录，是有意的：
  # ros2_ws/build、ros2_ws/install、ros2_ws/log 可能由 Docker 容器内的 root 用户生成。
  # 普通 ubuntu 用户删除这些文件会出现 Permission denied，导致同步失败。
  # 因此脚本只替换源码和配置，保留这些生成目录给 Docker/colcon 自己覆盖或复用。
  # 停止 ROS2 bridge。目录不存在或容器未运行时不视为失败。
  $remote_steps = @("cd '$RemoteProject/ros2' 2>/dev/null && docker compose down || true")
  # docker compose down 后马上离开 ros2 目录。
  # 后面会删除并重新解压 ros2 源码目录，如果 shell 还停在被删除目录里，Linux 会报
  # Current working directory cannot be established。
  $remote_steps += "cd /tmp"
  # 确保目标目录存在。
  $remote_steps += "mkdir -p '$RemoteProject'"
  # 删除上位机源码目录。这些目录都应该由当前压缩包重新提供。
  $remote_steps += "rm -rf '$RemoteProject/apps' '$RemoteProject/config' '$RemoteProject/include' '$RemoteProject/modules' '$RemoteProject/src' '$RemoteProject/systemd' '$RemoteProject/tests' '$RemoteProject/tools' '$RemoteProject/ros2' '$RemoteProject/ros2_ws/src'"
  # 删除普通顶层文件，避免旧 README 或旧 CMake 配置残留。
  $remote_steps += "rm -f '$RemoteProject/CMakeLists.txt' '$RemoteProject/README.md' '$RemoteProject/hardware.md' '$RemoteProject/operations.md' '$RemoteProject/modules.md' '$RemoteProject/raspberry-pi-debug.md' '$RemoteProject/hermes-voice-recovery.md'"
  # 解压 rpi_host 到目标目录。压缩包内部第一层是 rpi_host，所以使用 --strip-components=1 去掉这一层。
  $remote_steps += "tar -xzf '$remote_archive' -C '$RemoteProject' --strip-components=1"
  # 解压完成后删除树莓派上的临时压缩包。
  $remote_steps += "rm -f '$remote_archive'"
  # 删除树莓派本地 CMake 生成目录，避免旧缓存影响构建。
  $remote_steps += "cmake -E remove_directory '$RemoteProject/build'"
  # 在树莓派上配置和构建 C++ 上位机工具。
  $remote_steps += "cmake -S '$RemoteProject' -B '$RemoteProject/build'"
  $remote_steps += "cmake --build '$RemoteProject/build' -j4"
  # 运行协议单元测试。失败时脚本会停止，不会继续重启 ROS2 bridge。
  $remote_steps += "ctest --test-dir '$RemoteProject/build' --output-on-failure"
  # 恢复脚本由 systemd 直接执行，解压后确保它保留可执行权限。
  $remote_steps += "chmod +x '$RemoteProject/tools/recover_mcu_usb.sh'"
  # 重建并后台启动 ROS2 bridge。
  # 语音服务现由 ROS2 容器统一管理；停止旧宿主机服务，避免争抢麦克风。
  $remote_steps += "systemctl --user disable --now hermes-car-voice.service || true"
  $remote_steps += "cd '$RemoteProject/ros2'"
  $remote_steps += "docker compose build"
  $remote_steps += "docker compose up -d --force-recreate"
  $remote_command = $remote_steps -join " && "

  # 使用 ssh 在树莓派上执行上面的整段命令。
  $ssh_args = @("-t", "-p", $SshPort.ToString(), $remote_target, $remote_command)
  & ssh @ssh_args
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed: ssh"
  }
  Write-Host "[4/4] ROS2 bridge rebuilt and restarted."
} finally {
  # 无论成功还是失败，都清理本地临时压缩包。
  if (Test-Path -LiteralPath $archive_path) {
    Remove-Item -LiteralPath $archive_path -Force
  }
}

