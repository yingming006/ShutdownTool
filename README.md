# Windows 定时关机工具

一个简单的 Windows 定时关机工具，支持定时关机、重启和睡眠功能。

## 功能

- 定时关机 / 重启 / 睡眠
- 天、时、分三级倒计时设置
- 暂停 / 继续倒计时
- 结束前 55 秒弹窗提醒
- 最小化到系统托盘
- 单实例运行

## 截图

![Preview](./preview.png)

## 编译

**无需安装任何编译器！** Windows 自带 C# 编译器 `csc.exe`，一条命令即可编译。

```bash
# 一键构建
build_cs.bat

# 或手动一条命令
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /target:winexe /win32icon:icon.ico /out:shutdown_tool.exe shutdown_tool.cs
```

> 体积 ~18 KB，零外部依赖，Windows 7+ 开箱即用。

## 下载

[蓝奏云](https://wwpr.lanzout.com/b00q0d37yf) 密码: `8jdk`

## 开源协议

MIT License