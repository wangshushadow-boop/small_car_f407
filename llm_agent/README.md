# 小车大模型与 Agent

该目录独立存放小车大模型、Agent、记忆系统和工具调用相关内容，避免与
STM32 固件及 ROS 2 底盘代码混在一起。

WSL 从零构建、整体备份和跨主机迁移步骤见：
[WSL 大模型环境构建与迁移手册](docs/wsl_deployment_and_migration.md)。

目前已部署的本地模型服务：

- WSL 发行版：`Ubuntu-24.04`
- WSL 用户：`llm_agent`
- 用户主目录：`/home/llm_agent`
- Python 环境：`/opt/minicpm-service/venv`
- 模型目录：`/opt/models/MiniCPM-o-4_5-AWQ`
- API 地址：`http://127.0.0.1:8000/v1`
- API 模型名：`minicpm-o-4.5-awq`

## 1. 从 Windows 进入 WSL

在 Windows PowerShell 或 Windows Terminal 中执行：

```powershell
wsl -d Ubuntu-24.04 -u llm_agent
```

进入后，提示符会变成类似：

```text
llm_agent@主机名:/mnt/d/work/smart_car$
```

项目的 Windows 路径与 WSL 路径对应关系如下：

```text
D:\work\smart_car\llm_agent
/mnt/d/work/smart_car/llm_agent
```

进入脚本目录：

```bash
cd /mnt/d/work/smart_car/llm_agent
```

## 2. 进入模型 Python 环境

如果需要直接执行 Python、vLLM 或检查依赖，激活模型虚拟环境：

```bash
source /opt/minicpm-service/venv/bin/activate
```

激活后可检查环境：

```bash
which python
python --version
python -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name())"
vllm --version
```

退出 Python 虚拟环境：

```bash
deactivate
```

说明：运行本目录的脚本不需要手工激活虚拟环境，脚本会直接调用虚拟环境中的
`vllm` 和 `python`。

### 默认终端环境

WSL 用户 `llm_agent` 默认使用 Zsh，配置文件为 `/home/llm_agent/.zshrc`，
仓库中的配置模板为 `llm_agent/config/zshrc`。基础功能包括：

- Zsh 原生命令补全；
- 历史记录共享和方向键历史搜索；
- `zsh-autosuggestions` 命令自动建议；
- `zsh-syntax-highlighting` 命令语法高亮；
- `fzf` 模糊历史、文件及目录搜索。

如果修改了仓库模板，可在 WSL 中重新安装配置：

```bash
install -m 0644 /mnt/d/work/smart_car/llm_agent/config/zshrc ~/.zshrc
exec zsh
```

## 3. 启动模型

先设置一个本机 API 密钥，再启动：

```bash
export MINICPM_API_KEY='请替换为随机且足够长的密钥'
./scripts/start_minicpm.sh
```

首次启动通常需要约 1～2 分钟。启动脚本以前台方式运行，请保持这个 WSL
终端打开；按 `Ctrl+C` 可以停止模型。需要执行状态检查时，请另开一个
PowerShell/Windows Terminal 标签页，再次进入 WSL。

WSL 与普通 Linux 服务器不同：启动 WSL 的 Windows 客户端全部退出后，WSL
可能会清理孤立的 `nohup` 后台进程。因此脚本不使用不可靠的后台脱离方式。
日志位于：

```text
/opt/minicpm-service/logs/minicpm-o.stdout.log
/opt/minicpm-service/logs/minicpm-o.stderr.log
```

## 4. 检查状态和测试对话

```bash
export MINICPM_API_KEY='与启动时相同的密钥'
./scripts/status_minicpm.sh
./scripts/chat_test.sh
```

查看实时日志：

```bash
tail -f /opt/minicpm-service/logs/minicpm-o.stdout.log
```

查看显存：

```bash
watch -n 1 nvidia-smi
```

## 5. 停止模型

```bash
./scripts/stop_minicpm.sh
```

## 6. 供 Agent/Hermes 调用

兼容 OpenAI API 的连接参数：

```text
Base URL: http://127.0.0.1:8000/v1
Model: minicpm-o-4.5-awq
API Key: MINICPM_API_KEY 设置的值
```

后续 Agent 代码建议继续按以下结构添加：

```text
llm_agent/
├── agent/          # Agent 循环、模型路由和工具调度
├── memory/         # 长期记忆与短期上下文管理
├── tools/          # ROS 2、小车控制、视觉及网络工具
├── config/         # 不含密钥的配置模板
└── scripts/        # 模型和 Agent 运维脚本
```

不要把真实 API 密钥提交到 Git。后续可使用本地 `.env` 文件，并把它加入
`.gitignore`。
