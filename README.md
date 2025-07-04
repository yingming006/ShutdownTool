# Windows定时关机工具

一个简单的 Windows 定时关机工具，支持定时关机、重启和睡眠功能。

## 截图

![Preview](./preview.png)

## 编译命令

```bash
windres resource.rc -o resource.o

gcc shutdown_tool.c resource.o -o shutdown_tool.exe -Os -s -mwindows -municode -luser32 -lgdi32 -lshell32 -lcomctl32

upx --best shutdown_tool.exe
```

## 下载链接

[蓝奏云](https://wwpr.lanzout.com/b00q0d37yf) 密码: `8jdk`

## 开源协议

MIT License