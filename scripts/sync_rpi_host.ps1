param(
  [string]$HostAddress = "192.168.3.85",
  [string]$UserName = "ubuntu",
  [string]$RemoteProject = "/home/ubuntu/small_car_f407/rpi_host",
  [int]$SshPort = 22
)

$ErrorActionPreference = "Stop"

$project_root = Split-Path -Parent $PSScriptRoot
$archive_path = Join-Path $env:TEMP (
  "small_car_rpi_host_{0}.tar.gz" -f [guid]::NewGuid().ToString("N"))
$remote_archive = "/tmp/small_car_rpi_host.tar.gz"
$remote_target = "${UserName}@${HostAddress}"

function Invoke-CheckedCommand {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Command,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
  )

  & $Command @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed: $Command $($Arguments -join ' ')"
  }
}

try {
  Write-Host "[1/4] Packing rpi_host sources..."
  Push-Location $project_root
  try {
    Invoke-CheckedCommand tar -czf $archive_path `
      --exclude=rpi_host/build `
      --exclude=rpi_host/build-* `
      --exclude=rpi_host/ros2_ws/build `
      --exclude=rpi_host/ros2_ws/install `
      --exclude=rpi_host/ros2_ws/log `
      --exclude=rpi_host/.cache `
      --exclude=rpi_host/*.jpg `
      --exclude=rpi_host/*.wav `
      rpi_host
  } finally {
    Pop-Location
  }

  Write-Host "[2/4] Uploading to ${remote_target}..."
  Invoke-CheckedCommand scp -P $SshPort $archive_path `
    "${remote_target}:${remote_archive}"

  Write-Host "[3/4] Building host tools on Raspberry Pi..."
  $remote_command = @(
    "mkdir -p '$RemoteProject'",
    "tar -xzf '$remote_archive' -C '$RemoteProject' --strip-components=1",
    "rm -f '$remote_archive'",
    "cmake -E remove_directory '$RemoteProject/build'",
    "cmake -S '$RemoteProject' -B '$RemoteProject/build'",
    "cmake --build '$RemoteProject/build' -j4",
    "ctest --test-dir '$RemoteProject/build' --output-on-failure",
    "cd '$RemoteProject/ros2'",
    "docker compose build",
    "docker compose up -d --force-recreate"
  ) -join " && "

  Invoke-CheckedCommand ssh -t -p $SshPort $remote_target $remote_command
  Write-Host "[4/4] ROS2 bridge rebuilt and restarted."
} finally {
  if (Test-Path -LiteralPath $archive_path) {
    Remove-Item -LiteralPath $archive_path -Force
  }
}
