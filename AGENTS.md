# Codex 项目说明

本文件供 Codex 或其他 AI 编程助手在处理本仓库前阅读。这里记录项目约定、构建方式和修改注意事项。

## 项目概况

- 项目名称：`small_car_f407`
- 目标平台：STM32F407
- 工程来源：STM32CubeMX 生成
- 构建系统：CMake + Ninja
- 主要中间件：FreeRTOS / CMSIS-RTOS2
- 编译工具链：`arm-none-eabi-gcc`

## 常用命令

Debug 构建：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Release 构建：

```powershell
cmake --preset Release
cmake --build --preset Release
```

如果 CMake 提示找不到 `arm-none-eabi-gcc` 或 `arm-none-eabi-g++`，说明 ARM GCC 工具链未安装或未加入 `PATH`。

## 目录约定

- `Core/Inc`：应用头文件和 CubeMX 生成头文件。
- `Core/Src`：应用源码和 CubeMX 生成源码。
- `Drivers/`：STM32 HAL 与 CMSIS 文件，通常不手工修改。
- `Middlewares/`：第三方中间件，通常不手工修改。
- `cmake/`：工具链和 CubeMX CMake 集成文件。
- `small_car_f407.ioc`：CubeMX 配置文件。

## 修改规则

- 修改 CubeMX 可能重新生成的文件时，只在 `USER CODE BEGIN/END` 区域内添加自定义代码。
- 不对 CubeMX 生成文件做整文件格式化，避免引入大量无关 diff。
- 新增业务模块优先放在 `Core/Inc` 和 `Core/Src`。
- 不提交 `build/`、`.elf`、`.bin`、`.hex`、`.map`、目标文件或 IDE 本地配置。
- 硬件资料默认位于仓库外，只有需要版本管理的原理图、接口说明或资源分配表才加入仓库。

## 编码规范

- 人工编写的新 C/C++ 代码采用 Google 风格，配置见 `.clang-format`。
- C 代码使用 C11。
- 缩进使用 2 个空格，不使用 Tab。
- HAL/CubeMX 回调和生成函数保持原有 STM32 命名。
- 私有函数和局部变量优先使用 `snake_case`。
- 宏和编译期常量使用 `UPPER_SNAKE_CASE`。
- 文件内私有函数和数据优先使用 `static`。

## 验证建议

完成代码修改后，优先执行 Debug 构建。涉及优化、体积或发布固件时，再执行 Release 构建。
