#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/subsystem:windows /entry:wWinMainCRTStartup")

// 定义控件 ID
#define ID_DAY 1001
#define ID_HOUR 1002
#define ID_MINUTE 1003
#define ID_SCHEDULE 1004
#define ID_CANCEL 1005
#define ID_SHUTDOWN 1006
#define ID_RESTART 1007
#define ID_SLEEP 1008
#define ID_TIMER 1009
#define ID_COUNTDOWN 1010

// 全局变量
HWND hDay, hHour, hMinute;
HWND hShutdownRadio, hRestartRadio, hSleepRadio;
HWND hCountdownLabel; // 倒计时显示控件
HFONT hGlobalFont; // 全局字体
HBRUSH hBackgroundBrush; // 背景画刷
int remainingSeconds = 0; // 剩余秒数

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
void ScheduleShutdown(int days, int hours, int minutes, int operation) {
    remainingSeconds = days * 86400 + hours * 3600 + minutes * 60;
    if (remainingSeconds <= 0) {
        return;
    }

    wchar_t command[50];
    if (operation == ID_SHUTDOWN) {
        swprintf(command, 50, L"shutdown -s -t %d", remainingSeconds);
    } else if (operation == ID_RESTART) {
        swprintf(command, 50, L"shutdown -r -t %d", remainingSeconds);
    } else if (operation == ID_SLEEP) {
        swprintf(command, 50, L"shutdown -h -t %d", remainingSeconds);
    }

    if (RunProcess(command)) {
        HWND hwnd = GetParent(hCountdownLabel);
        SetTimer(hwnd, ID_TIMER, 1000, NULL);
    }
}

// 取消关机函数
void CancelShutdownTool() {
    if (RunProcess(L"shutdown -a")) {
        remainingSeconds = 0;
        HWND hwnd = GetParent(hCountdownLabel);
        KillTimer(hwnd, ID_TIMER);
        ShowWindow(hCountdownLabel, SW_HIDE);
    }
}

// 填充下拉框选项
void FillComboBox(HWND hComboBox, int start, int end) {
    for (int i = start; i <= end; i++) {
        wchar_t buffer[10];
        swprintf(buffer, 10, L"%02d", i);
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)buffer);
    }
    SendMessage(hComboBox, CB_SETCURSEL, 0, 0); // 默认选中第一个选项
}

// 更新倒计时显示
void UpdateCountdown(HWND hwnd) {
    if (remainingSeconds <= 0) {
        ShowWindow(hCountdownLabel, SW_HIDE); // 倒计时结束，隐藏文字
        return;
    }

    int days = remainingSeconds / 86400;
    int hours = (remainingSeconds % 86400) / 3600;
    int minutes = (remainingSeconds % 3600) / 60;
    int seconds = remainingSeconds % 60;

    wchar_t countdownText[100];
    swprintf(countdownText, 100, L"倒计时: %02d 天 %02d 时 %02d 分 %02d 秒", days, hours, minutes, seconds);
    SetWindowTextW(hCountdownLabel, countdownText);
    ShowWindow(hCountdownLabel, SW_SHOW); // 显示倒计时文字
    InvalidateRect(hCountdownLabel, NULL, TRUE); // 强制重绘倒计时标签
    UpdateWindow(hCountdownLabel); // 立即更新窗口
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // 计算单选按钮的总宽度和居中位置
            int radioWidth = 80; // 每个单选按钮的宽度
            int radioSpacing = 10; // 单选按钮之间的间距
            int totalRadioWidth = 3 * radioWidth + 2 * radioSpacing; // 总宽度
            int startX = (500 - totalRadioWidth) / 2; // 起始X坐标

            // 创建单选按钮
            hShutdownRadio = CreateWindowW(L"BUTTON", L"关机",
                                          WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                                          startX, 20, radioWidth, 30, hwnd, (HMENU)ID_SHUTDOWN, NULL, NULL);
            SendMessage(hShutdownRadio, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hRestartRadio = CreateWindowW(L"BUTTON", L"重启",
                                         WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                         startX + radioWidth + radioSpacing, 20, radioWidth, 30, hwnd, (HMENU)ID_RESTART, NULL, NULL);
            SendMessage(hRestartRadio, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hSleepRadio = CreateWindowW(L"BUTTON", L"睡眠",
                                       WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                       startX + 2 * (radioWidth + radioSpacing), 20, radioWidth, 30, hwnd, (HMENU)ID_SLEEP, NULL, NULL);
            SendMessage(hSleepRadio, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 默认选中关机
            SendMessage(hShutdownRadio, BM_SETCHECK, BST_CHECKED, 0);

            // 创建天下拉框
            hDay = CreateWindowW(L"COMBOBOX", NULL,
                                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                100, 60, 60, 200, hwnd, (HMENU)ID_DAY, NULL, NULL);
            FillComboBox(hDay, 0, 99); // 填充 00-99
            SendMessage(hDay, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建天提示语
            HWND hLabelDay = CreateWindowW(L"STATIC", L"天",
                                          WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          170, 65, 30, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelDay, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建小时下拉框
            hHour = CreateWindowW(L"COMBOBOX", NULL,
                                 WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 210, 60, 60, 200, hwnd, (HMENU)ID_HOUR, NULL, NULL);
            FillComboBox(hHour, 0, 23); // 填充 00-23
            SendMessage(hHour, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建小时提示语
            HWND hLabelHour = CreateWindowW(L"STATIC", L"时",
                                           WS_VISIBLE | WS_CHILD | SS_LEFT,
                                           280, 65, 30, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelHour, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建分钟下拉框
            hMinute = CreateWindowW(L"COMBOBOX", NULL,
                                   WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   320, 60, 60, 200, hwnd, (HMENU)ID_MINUTE, NULL, NULL);
            FillComboBox(hMinute, 0, 59); // 填充 00-59
            SendMessage(hMinute, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建分钟提示语
            HWND hLabelMinute = CreateWindowW(L"STATIC", L"分",
                                             WS_VISIBLE | WS_CHILD | SS_LEFT,
                                             390, 65, 30, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelMinute, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建倒计时显示控件（放在按钮上方）
            hCountdownLabel = CreateWindowW(L"STATIC", L"",
                                           WS_VISIBLE | WS_CHILD | SS_CENTER,
                                           50, 100, 400, 40, hwnd, (HMENU)ID_COUNTDOWN, NULL, NULL);
            HFONT hCountdownFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                             DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
            SendMessage(hCountdownLabel, WM_SETFONT, (WPARAM)hCountdownFont, TRUE);
            ShowWindow(hCountdownLabel, SW_HIDE); // 初始隐藏

            // 创建按钮
            HWND hButtonSchedule = CreateWindowW(L"BUTTON", L"确定",
                                               WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                               150, 150, 100, 30, hwnd, (HMENU)ID_SCHEDULE, NULL, NULL);
            SendMessage(hButtonSchedule, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hButtonCancel = CreateWindowW(L"BUTTON", L"清除",
                                             WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                             260, 150, 100, 30, hwnd, (HMENU)ID_CANCEL, NULL, NULL);
            SendMessage(hButtonCancel, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_SCHEDULE) {
                // 获取选中的天、时、分
                int day = SendMessage(hDay, CB_GETCURSEL, 0, 0);
                int hour = SendMessage(hHour, CB_GETCURSEL, 0, 0);
                int minute = SendMessage(hMinute, CB_GETCURSEL, 0, 0);

                // 获取选中的操作
                int operation = 0;
                if (SendMessage(hShutdownRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    operation = ID_SHUTDOWN;
                } else if (SendMessage(hRestartRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    operation = ID_RESTART;
                } else if (SendMessage(hSleepRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    operation = ID_SLEEP;
                }

                ScheduleShutdown(day, hour, minute, operation);
                UpdateCountdown(hwnd); // 更新倒计时显示
            } else if (LOWORD(wParam) == ID_CANCEL) {
                CancelShutdownTool();
            }
            break;
        }
        case WM_TIMER: {
            if (wParam == ID_TIMER) {
                if (remainingSeconds > 0) {
                    remainingSeconds--;
                    UpdateCountdown(hwnd); // 更新倒计时显示
                } else {
                    KillTimer(hwnd, ID_TIMER); // 停止定时器
                    ShowWindow(hCountdownLabel, SW_HIDE); // 隐藏倒计时文字
                }
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            // 获取鼠标滚轮消息的目标控件
            HWND hFocus = GetFocus();
            if (hFocus == hDay || hFocus == hHour || hFocus == hMinute) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int currentIndex = SendMessage(hFocus, CB_GETCURSEL, 0, 0);
                int newIndex = currentIndex - (delta / WHEEL_DELTA);
                int count = SendMessage(hFocus, CB_GETCOUNT, 0, 0);
                if (newIndex < 0) newIndex = 0;
                if (newIndex >= count) newIndex = count - 1;
                SendMessage(hFocus, CB_SETCURSEL, newIndex, 0);
            }
            break;
        }
        case WM_ERASEBKGND: {
            // 设置窗口背景色为灰色
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, hBackgroundBrush);
            return 1; // 表示背景已处理
        }
        case WM_CTLCOLORSTATIC: {
            // 设置静态文本控件的背景色为灰色，文本颜色为黑色
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(240, 240, 240)); // 灰色背景
            SetTextColor(hdc, RGB(0, 0, 0)); // 黑色文本
            return (LRESULT)hBackgroundBrush;
        }
        case WM_CTLCOLORBTN: {
            // 设置按钮控件的背景色为灰色
            return (LRESULT)hBackgroundBrush;
        }
        case WM_DESTROY: {
            // 释放全局字体和背景画刷
            DeleteObject(hGlobalFont);
            DeleteObject(hBackgroundBrush);
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

    // 创建灰色背景画刷
    hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240)); // 灰色

    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ShutdownToolClass";
    RegisterClassW(&wc);

    // 计算窗口居中位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);  // 屏幕宽度
    int screenHeight = GetSystemMetrics(SM_CYSCREEN); // 屏幕高度
    int windowWidth = 500;  // 窗口宽度
    int windowHeight = 250; // 窗口高度增加以容纳倒计时文字
    int x = (screenWidth - windowWidth) / 2;  // 窗口左上角 X 坐标
    int y = (screenHeight - windowHeight) / 2; // 窗口左上角 Y 坐标

    // 创建窗口
    HWND hwnd = CreateWindowExW(0, L"ShutdownToolClass", L"定时关机工具",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // 移除 WS_THICKFRAME 和 WS_MAXIMIZEBOX
                               x, y, windowWidth, windowHeight,
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