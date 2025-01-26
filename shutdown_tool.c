#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "resource.h"

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

// 添加系统托盘相关定义
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1

// 全局变量
HWND hDay, hHour, hMinute;
HWND hShutdownRadio, hRestartRadio, hSleepRadio;
HWND hCountdownLabel; // 倒计时显示控件
HFONT hGlobalFont; // 全局字体
HBRUSH hBackgroundBrush; // 背景画刷
int remainingSeconds = 0; // 剩余秒数
HFONT hCountdownFont = NULL; // 添加全局变量用于跟踪倒计时字体
BOOL isTimerRunning = FALSE;

NOTIFYICONDATA nid = {0};

// 在全局变量声明部分添加
#define REG_PATH L"Software\\ShutdownTool"
#define REG_NAME L"ShutdownTime"

// 函数声明
BOOL RunProcess(LPCWSTR command);
void ShowErrorMessage(HWND hwnd, const wchar_t* message);
void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation);
void CancelShutdownTool(void);
void FillComboBox(HWND hComboBox, int start, int end);
void UpdateCountdown(HWND hwnd);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 添加以下函数声明
void SaveShutdownTime(SYSTEMTIME targetTime);
BOOL LoadShutdownTime(SYSTEMTIME* targetTime);
void ClearShutdownTime(void);
void CalculateRemainingTime(const SYSTEMTIME* targetTime);

// 函数实现
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

// 添加一个显示错误消息的辅助函数
void ShowErrorMessage(HWND hwnd, const wchar_t* message) {
    MessageBoxW(hwnd, message, L"错误", MB_OK | MB_ICONERROR);
}

// 添加以下函数实现
void SaveShutdownTime(SYSTEMTIME targetTime) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, 
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, REG_NAME, 0, REG_BINARY, 
            (BYTE*)&targetTime, sizeof(SYSTEMTIME));
        RegCloseKey(hKey);
    }
}

BOOL LoadShutdownTime(SYSTEMTIME* targetTime) {
    HKEY hKey;
    DWORD size = sizeof(SYSTEMTIME);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LONG result = RegQueryValueExW(hKey, REG_NAME, NULL, NULL, 
            (BYTE*)targetTime, &size);
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
    return FALSE;
}

void ClearShutdownTime(void) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, REG_NAME);
        RegCloseKey(hKey);
    }
}

void CalculateRemainingTime(const SYSTEMTIME* targetTime) {
    SYSTEMTIME currentTime;
    FILETIME currentFT, targetFT;
    ULARGE_INTEGER current, target;

    GetLocalTime(&currentTime);
    SystemTimeToFileTime(&currentTime, &currentFT);
    SystemTimeToFileTime(targetTime, &targetFT);

    current.LowPart = currentFT.dwLowDateTime;
    current.HighPart = currentFT.dwHighDateTime;
    target.LowPart = targetFT.dwLowDateTime;
    target.HighPart = targetFT.dwHighDateTime;

    if (target.QuadPart <= current.QuadPart) {
        remainingSeconds = 0;
        return;
    }

    // 计算剩余秒数
    remainingSeconds = (int)((target.QuadPart - current.QuadPart) / 10000000LL);
}

// 修改 ScheduleShutdown 函数
void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation) {
    if (isTimerRunning) {
        if (MessageBoxW(hwnd, L"已有定时任务在运行，是否重新设置？", 
                       L"提示", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
        CancelShutdownTool();
    }

    remainingSeconds = days * 86400 + hours * 3600 + minutes * 60;
    if (remainingSeconds <= 0) {
        ShowErrorMessage(hwnd, L"请设置大于0的时间！");
        return;
    }

    // 计算目标时间
    SYSTEMTIME targetTime;
    GetLocalTime(&targetTime);
    ULARGE_INTEGER targetUI;
    FILETIME targetFT;
    SystemTimeToFileTime(&targetTime, &targetFT);
    targetUI.LowPart = targetFT.dwLowDateTime;
    targetUI.HighPart = targetFT.dwHighDateTime;
    targetUI.QuadPart += (ULONGLONG)remainingSeconds * 10000000LL;
    targetFT.dwLowDateTime = targetUI.LowPart;
    targetFT.dwHighDateTime = targetUI.HighPart;
    FileTimeToSystemTime(&targetFT, &targetTime);

    // 保存目标时间
    SaveShutdownTime(targetTime);

    wchar_t command[50];
    if (operation == ID_SHUTDOWN) {
        swprintf(command, 50, L"shutdown -s -t %d", remainingSeconds);
    } else if (operation == ID_RESTART) {
        swprintf(command, 50, L"shutdown -r -t %d", remainingSeconds);
    } else if (operation == ID_SLEEP) {
        swprintf(command, 50, L"shutdown -h -t %d", remainingSeconds);
    }

    if (!RunProcess(command)) {
        ShowErrorMessage(hwnd, L"设置定时任务失败！");
        ClearShutdownTime();
        return;
    }

    SetTimer(hwnd, ID_TIMER, 1000, NULL);
    isTimerRunning = TRUE;
}

// 修改 CancelShutdownTool 函数
void CancelShutdownTool(void) {
    if (!isTimerRunning) return;

    if (RunProcess(L"shutdown -a")) {
        remainingSeconds = 0;
        HWND hwnd = GetParent(hCountdownLabel);
        KillTimer(hwnd, ID_TIMER);
        ShowWindow(hCountdownLabel, SW_HIDE);
        isTimerRunning = FALSE;
        ClearShutdownTime();
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
            hCountdownFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                         DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
            if (hCountdownFont) {
                SendMessage(hCountdownLabel, WM_SETFONT, (WPARAM)hCountdownFont, TRUE);
            }
            ShowWindow(hCountdownLabel, SW_HIDE);

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
            // 添加按钮点击效果
            if (HIWORD(wParam) == BN_CLICKED) {
                HWND hButton = (HWND)lParam;
                // 临时禁用按钮防止重复点击
                EnableWindow(hButton, FALSE);
                // 处理点击事件
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

                    ScheduleShutdown(hwnd, day, hour, minute, operation);
                    UpdateCountdown(hwnd); // 更新倒计时显示
                } else if (LOWORD(wParam) == ID_CANCEL) {
                    CancelShutdownTool();
                }
                // 重新启用按钮
                EnableWindow(hButton, TRUE);
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
            if (hGlobalFont) DeleteObject(hGlobalFont);
            if (hCountdownFont) DeleteObject(hCountdownFont);
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            PostQuitMessage(0);
            break;
        }
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                // 最小化到系统托盘
                ShowWindow(hwnd, SW_HIDE);
                nid.cbSize = sizeof(NOTIFYICONDATA);
                nid.hWnd = hwnd;
                nid.uID = ID_TRAYICON;
                nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                nid.uCallbackMessage = WM_TRAYICON;
                // 使用自定义图标
                nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
                wcscpy(nid.szTip, L"定时关机工具");
                Shell_NotifyIcon(NIM_ADD, &nid);
            }
            break;
        }
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDOWN) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                Shell_NotifyIcon(NIM_DELETE, &nid);
            }
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
    // 添加图标
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
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

    // 尝试加载保存的关机时间
    SYSTEMTIME targetTime;
    if (LoadShutdownTime(&targetTime)) {
        CalculateRemainingTime(&targetTime);
        if (remainingSeconds > 0) {
            // 如果还有剩余时间，启动定时器
            SetTimer(hwnd, ID_TIMER, 1000, NULL);
            isTimerRunning = TRUE;
            UpdateCountdown(hwnd);
        } else {
            // 如果时间已过，清除保存的时间
            ClearShutdownTime();
        }
    }

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