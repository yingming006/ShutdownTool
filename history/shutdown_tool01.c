#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/subsystem:windows /entry:wWinMainCRTStartup")

// 定义控件 ID
#define ID_INPUT 1001
#define ID_SCHEDULE 1002
#define ID_CANCEL 1003

// 全局变量
HWND hInput;
HFONT hGlobalFont; // 全局字体

// 启动进程（无控制台窗口）
BOOL RunProcess(LPCWSTR command) {
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // 隐藏窗口

    // 将命令行复制到可写的缓冲区
    wchar_t cmd[256];
    wcsncpy(cmd, command, 255);
    cmd[255] = L'\0';

    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return TRUE;
    }
    return FALSE;
}

// 定时关机函数
void ScheduleShutdown(int minutes) {
    if (minutes <= 0) {
        MessageBoxW(NULL, L"请输入一个大于0的整数", L"错误", MB_ICONERROR);
        return;
    }

    int seconds = minutes * 60;
    wchar_t command[50];
    swprintf(command, 50, L"shutdown -s -t %d", seconds);

    if (RunProcess(command)) {
        wchar_t message[100];
        swprintf(message, 100, L"计算机将在 %d 分钟后关机", minutes);
        MessageBoxW(NULL, message, L"成功", MB_ICONINFORMATION);
    } else {
        MessageBoxW(NULL, L"定时关机失败", L"错误", MB_ICONERROR);
    }
}

// 取消关机函数
void CancelShutdownTool() {
    if (RunProcess(L"shutdown -a")) {
        MessageBoxW(NULL, L"已取消定时关机", L"成功", MB_ICONINFORMATION);
    } else {
        MessageBoxW(NULL, L"取消关机失败", L"错误", MB_ICONERROR);
    }
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // 创建提示语
            HWND hLabel = CreateWindowW(L"STATIC", L"请输入关机时间（分钟）：",
                                       WS_VISIBLE | WS_CHILD | SS_LEFT,
                                       10, 10, 200, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建输入框
            hInput = CreateWindowExW(0, L"EDIT", NULL,
                                    WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
                                    10, 30, 200, 30, hwnd, (HMENU)ID_INPUT, NULL, NULL);
            SendMessage(hInput, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建按钮
            HWND hButtonSchedule = CreateWindowW(L"BUTTON", L"定时关机",
                                               WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                               10, 70, 100, 30, hwnd, (HMENU)ID_SCHEDULE, NULL, NULL);
            SendMessage(hButtonSchedule, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hButtonCancel = CreateWindowW(L"BUTTON", L"取消关机",
                                             WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                             120, 70, 100, 30, hwnd, (HMENU)ID_CANCEL, NULL, NULL);
            SendMessage(hButtonCancel, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);
            break;
        }
        case WM_CTLCOLORSTATIC: {
            // 设置提示语的背景为透明
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SCHEDULE) {
                wchar_t buffer[10];
                GetWindowTextW(hInput, buffer, 10);
                int minutes = _wtoi(buffer);
                ScheduleShutdown(minutes);
            } else if (LOWORD(wParam) == ID_CANCEL) {
                CancelShutdownTool();
            }
            break;
        }
        case WM_DESTROY: {
            // 释放全局字体
            DeleteObject(hGlobalFont);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// 主函数
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 创建全局字体
    hGlobalFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ShutdownToolClass";
    RegisterClassW(&wc);

    // 计算窗口居中位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);  // 屏幕宽度
    int screenHeight = GetSystemMetrics(SM_CYSCREEN); // 屏幕高度
    int windowWidth = 250;  // 窗口宽度
    int windowHeight = 150; // 窗口高度
    int x = (screenWidth - windowWidth) / 2;  // 窗口左上角 X 坐标
    int y = (screenHeight - windowHeight) / 2; // 窗口左上角 Y 坐标

    // 创建窗口
    HWND hwnd = CreateWindowExW(0, L"ShutdownToolClass", L"定时关机工具",
                               WS_OVERLAPPEDWINDOW, x, y, windowWidth, windowHeight,
                               NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        return 0;
    }

    // 设置窗口字体
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}