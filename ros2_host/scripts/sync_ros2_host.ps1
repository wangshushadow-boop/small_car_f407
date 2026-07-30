# 打包并上传 ros2_host。
# 停止旧容器。
[CmdletBinding()]
param(
  # 上位机地址。
  [string]$HostAddress = "192.168.3.85",

  # SSH 登录用户名。
  [string]$UserName = "ubuntu",

  # 上位机上的部署目录。
  [string]$RemoteProject = "/home/ubuntu/small_car_f407/ros2_host",

  # SSH 端口。
  [int]$SshPort = 22
)

# 任意命令失败后立即停止。
$ErrorActionPreference = "Stop"

# 脚本位于 ros2_host/scripts，父目录就是需要部署的 ros2_host。
$sourceRoot = Split-Path -Parent $PSScriptRoot
$workspaceRoot = Split-Path -Parent $sourceRoot
$sourceName = Split-Path -Leaf $sourceRoot

# 创建本地临时压缩包。
$archiveName = "small_car_ros2_host_$([guid]::NewGuid().ToString('N')).tar.gz"
$archivePath = Join-Path $env:TEMP $archiveName
$remoteArchive = "/tmp/$archiveName"
$remoteTarget = "${UserName}@${HostAddress}"

try {
  Write-Host "[1/4] 打包 ros2_host 源码..."

  # 从仓库根目录打包，压缩包的第一层目录为 ros2_host。
  Push-Location $workspaceRoot
  try {
    $tarArgs = @(
      "-czf", $archivePath,
      "--exclude=${sourceName}/build",
      "--exclude=${sourceName}/build-*",
      "--exclude=${sourceName}/install",
      "--exclude=${sourceName}/install-*",
      "--exclude=${sourceName}/log",
      "--exclude=${sourceName}/log-*",
      "--exclude=${sourceName}/.cache",
      "--exclude=${sourceName}/*.jpg",
      "--exclude=${sourceName}/*.wav",
      $sourceName
    )
    & tar @tarArgs
    if ($LASTEXITCODE -ne 0) {
      throw "打包失败"
    }
  } finally {
    Pop-Location
  }

  Write-Host "[2/4] 上传源码到 ${remoteTarget}..."

  & scp -P $SshPort $archivePath "${remoteTarget}:${remoteArchive}"
  if ($LASTEXITCODE -ne 0) {
    throw "上传失败"
  }

  Write-Host "[3/4] 在上位机编译并运行测试..."

  # 远端按顺序执行：停止容器、替换源码、构建测试、重新启动容器。
  $remoteSteps = @(
    "set -e",
    "if [ -f '$RemoteProject/ros2/compose.yaml' ]; then cd '$RemoteProject/ros2' && docker compose down; fi",
    "mkdir -p '$RemoteProject'",
    "cd /tmp",
    "rm -rf '$RemoteProject/apps' '$RemoteProject/config' '$RemoteProject/docs' '$RemoteProject/include' '$RemoteProject/modules' '$RemoteProject/ros2' '$RemoteProject/scripts' '$RemoteProject/src' '$RemoteProject/systemd' '$RemoteProject/tests' '$RemoteProject/tools'",
    "tar -xzf '$remoteArchive' -C '$RemoteProject' --strip-components=1",
    "rm -f '$remoteArchive'",
    "cmake -S '$RemoteProject' -B '$RemoteProject/build-host'",
    "cmake --build '$RemoteProject/build-host' -j4",
    "ctest --test-dir '$RemoteProject/build-host' --output-on-failure",
    "chmod +x '$RemoteProject/tools/recover_mcu_usb.sh'",
    "chmod +x '$RemoteProject/tools/mcu_ota.py' '$RemoteProject/scripts/update_mcu_firmware.sh'",
    "cd '$RemoteProject/ros2'",
    "docker compose up --build -d --force-recreate",
    "docker compose ps"
  )
  $remoteCommand = $remoteSteps -join " && "

  & ssh -p $SshPort $remoteTarget $remoteCommand
  if ($LASTEXITCODE -ne 0) {
    throw "上位机部署失败"
  }

  Write-Host "[4/4] 部署完成。"
  Write-Host "上位机目录：$RemoteProject"
} finally {
  # 无论成功或失败，都删除本地临时压缩包。
  if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
  }
}
