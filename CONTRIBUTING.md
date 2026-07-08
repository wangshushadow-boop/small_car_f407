# 贡献说明

## 提交前检查

1. 使用对应的 CMake preset 构建工程。
2. 检查代码差异，避免把无关的 CubeMX 重新生成内容放进同一次提交。
3. 确认没有暂存构建产物、固件文件或 IDE 本地配置。

## 推荐命令

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

如需检查 Release 版本体积：

```powershell
cmake --preset Release
cmake --build --preset Release
```

## 提交习惯

- 每次提交聚焦一个功能、修复或硬件变更。
- 提交信息中尽量说明受影响的外设、模块或硬件接口。
- 如果修改 `.ioc` 导致代码重新生成，请在提交说明中写明原因。
