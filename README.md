# Windows定时关机工具

一个简单的 Windows 定时关机工具，支持定时关机、重启和休眠功能。

## 功能特点

- 支持设置天、时、分的定时
- 支持关机、重启、休眠三种模式
- 支持最小化到系统托盘
- 自动保存定时任务，重启软件后恢复
- 支持鼠标滚轮调整时间
- 简洁的用户界面

## 使用方法

1. 选择需要的操作模式（关机/重启/休眠）
2. 设置定时时长（天/时/分）
3. 点击"确定"开始定时任务
4. 点击"清除"可以取消定时任务
5. 最小化时会自动隐藏到系统托盘

## 编译方法

使用 MinGW-w64 编译：

```bash
windres resource.rc -O coff -o resource.res
gcc -g shutdown_tool.c resource.res -o ShutdownTool.exe -mwindows -lcomctl32 -municode
```

## 注意事项

- 使用 Windows 注册表保存配置

## 开源协议

MIT License