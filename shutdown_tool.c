#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/subsystem:windows /entry:wWinMainCRTStartup")

// 定义控件ID
#define ID_DAY 1001        // 天数选择控件
#define ID_HOUR 1002       // 小时选择控件
#define ID_MINUTE 1003     // 分钟选择控件
#define ID_SCHEDULE 1004   // 确定按钮
#define ID_CANCEL 1005     // 清除按钮
#define ID_SHUTDOWN 1006   // 关机选项
#define ID_RESTART 1007    // 重启选项
#define ID_SLEEP 1008      // 睡眠选项
#define ID_TIMER 1009      // 计时器ID
#define ID_COUNTDOWN 1010  // 倒计时显示
#define ID_MINIMIZE 1012   // 最小化按钮
#define ID_EXIT 1013       // 退出按钮
#define ID_URL 1014        // 超链接控件
#define WM_TRAYICON (WM_USER + 1)  // 托盘消息
#define ID_TRAYICON 1      // 托盘图标ID

// 全局变量声明
static HWND hDay, hHour, hMinute;                    // 时间选择控件句柄
static HWND hShutdownRadio, hRestartRadio, hSleepRadio; // 操作选项单选按钮
static HWND hCountdownLabel;                         // 倒计时显示标签
static HFONT hGlobalFont = NULL;                     // 全局字体
static HBRUSH hBackgroundBrush = NULL;               // 背景画刷
static HFONT hCountdownFont = NULL;                  // 倒计时字体
static int remainingSeconds = 0;                     // 剩余秒数
static BOOL isTimerRunning = FALSE;                  // 计时器运行状态
static NOTIFYICONDATA nid = {0};                     // 托盘图标数据
static wchar_t lastCountdownText[100] = {0};         // 上次倒计时文本

// 函数声明
BOOL RunProcess(LPCWSTR command);                    // 执行系统命令
void ShowErrorMessage(HWND hwnd, const wchar_t* message); // 显示错误消息
void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation); // 设置定时任务
void CancelShutdownTool(HWND hwnd);                  // 取消定时任务
void FillComboBox(HWND hComboBox, int start, int end); // 填充下拉框选项
void UpdateCountdown(HWND hwnd);                     // 更新倒计时显示
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // 窗口消息处理
BOOL InitResources(void);                            // 初始化资源
void CleanupResources(void);                         // 清理资源

// 执行系统命令的函数
BOOL RunProcess(LPCWSTR command) {
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;                       // 隐藏命令窗口

    wchar_t cmd[256];
    wcscpy(cmd, command);
    cmd[255] = L'\0';

    // 创建新进程执行命令
    BOOL success = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return success;
}

// 显示错误消息框
void ShowErrorMessage(HWND hwnd, const wchar_t* message) {
    MessageBoxW(hwnd, message, L"错误", MB_OK | MB_ICONERROR);
}

// 设置定时关机/重启/睡眠任务
void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation) {
    // 如果已有任务，先取消
    if (isTimerRunning) {
        CancelShutdownTool(hwnd);
    }

    remainingSeconds = days * 86400 + hours * 3600 + minutes * 60;  // 计算总秒数
    if (remainingSeconds <= 0) return;

    wchar_t command[50];
    // 根据操作类型生成对应命令
    if (operation == ID_SHUTDOWN) {
        swprintf(command, 50, L"shutdown -s -t %d", remainingSeconds);  // 关机
    } else if (operation == ID_RESTART) {
        swprintf(command, 50, L"shutdown -r -t %d", remainingSeconds);  // 重启
    } else if (operation == ID_SLEEP) {
        swprintf(command, 50, L"shutdown -h -t %d", remainingSeconds);  // 睡眠
    }

    if (!RunProcess(command)) {
        ShowErrorMessage(hwnd, L"设置定时任务失败！");
        return;
    }

    SetTimer(hwnd, ID_TIMER, 1000, NULL);  // 设置每秒更新一次的计时器
    isTimerRunning = TRUE;
    UpdateCountdown(hwnd);                 // 更新倒计时显示
}

// 取消定时任务
void CancelShutdownTool(HWND hwnd) {
    if (!isTimerRunning) return;

    if (RunProcess(L"shutdown -a")) {      // 执行取消关机命令
        remainingSeconds = 0;
        KillTimer(hwnd, ID_TIMER);         // 停止计时器
        ShowWindow(hCountdownLabel, SW_HIDE); // 隐藏倒计时标签
        isTimerRunning = FALSE;
        lastCountdownText[0] = L'\0';
    }
}

// 填充下拉框选项
void FillComboBox(HWND hComboBox, int start, int end) {
    for (int i = start; i <= end; i++) {
        wchar_t buffer[10];
        swprintf(buffer, 10, L"%02d", i);  // 格式化为两位数
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)buffer);
    }
    SendMessage(hComboBox, CB_SETCURSEL, 0, 0);  // 默认选中第一个选项
}

// 更新倒计时显示
void UpdateCountdown(HWND hwnd) {
    if (remainingSeconds <= 0) {
        ShowWindow(hCountdownLabel, SW_HIDE);
        return;
    }

    // 计算剩余时间
    int days = remainingSeconds / 86400;
    int hours = (remainingSeconds % 86400) / 3600;
    int minutes = (remainingSeconds % 3600) / 60;
    int seconds = remainingSeconds % 60;

    wchar_t countdownText[100];
    swprintf(countdownText, 100, L"倒计时: %02d 天 %02d 时 %02d 分 %02d 秒", 
             days, hours, minutes, seconds);

    // 只有当文本变化时才更新显示
    if (wcscmp(countdownText, lastCountdownText) != 0) {
        SetWindowTextW(hCountdownLabel, countdownText);
        ShowWindow(hCountdownLabel, SW_SHOW);
        wcscpy(lastCountdownText, countdownText);
    }
}

// 初始化资源（字体、画刷等）
BOOL InitResources(void) {
    hGlobalFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
    if (!hGlobalFont) return FALSE;

    hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));  // 创建背景画刷
    if (!hBackgroundBrush) {
        DeleteObject(hGlobalFont);
        hGlobalFont = NULL;
        return FALSE;
    }

    hCountdownFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
    if (!hCountdownFont) {
        DeleteObject(hGlobalFont);
        DeleteObject(hBackgroundBrush);
        hGlobalFont = NULL;
        hBackgroundBrush = NULL;
        return FALSE;
    }
    return TRUE;
}

// 清理资源
void CleanupResources(void) {
    if (hGlobalFont) {
        DeleteObject(hGlobalFont);
        hGlobalFont = NULL;
    }
    if (hBackgroundBrush) {
        DeleteObject(hBackgroundBrush);
        hBackgroundBrush = NULL;
    }
    if (hCountdownFont) {
        DeleteObject(hCountdownFont);
        hCountdownFont = NULL;
    }
}

// 窗口消息处理函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            if (!hGlobalFont || !hBackgroundBrush || !hCountdownFont) {
                return -1;  // 资源未初始化成功
            }

            int windowWidth = 500;  // 减小窗口宽度
            int radioWidth = 80;
            int radioSpacing = 10;
            int totalRadioWidth = 3 * radioWidth + 2 * radioSpacing;
            int radioStartX = (windowWidth - totalRadioWidth) / 2;

            // 创建操作选项单选按钮
            hShutdownRadio = CreateWindowW(L"BUTTON", L"关机",
                                          WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                                          radioStartX, 20, radioWidth, 30, hwnd, (HMENU)ID_SHUTDOWN, NULL, NULL);
            SendMessage(hShutdownRadio, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hRestartRadio = CreateWindowW(L"BUTTON", L"重启",
                                         WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                         radioStartX + radioWidth + radioSpacing, 20, radioWidth, 30, hwnd, (HMENU)ID_RESTART, NULL, NULL);
            SendMessage(hRestartRadio, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hSleepRadio = CreateWindowW(L"BUTTON", L"睡眠",
                                       WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                                       radioStartX + 2 * (radioWidth + radioSpacing), 20, radioWidth, 30, hwnd, (HMENU)ID_SLEEP, NULL, NULL);
            SendMessage(hSleepRadio, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            SendMessage(hShutdownRadio, BM_SETCHECK, BST_CHECKED, 0);  // 默认选中关机

            int comboBoxWidth = 60;
            int labelWidth = 30;
            int totalComboWidth = 3 * comboBoxWidth + 2 * labelWidth + 20;
            int comboStartX = (windowWidth - totalComboWidth) / 2;

            // 创建时间选择下拉框
            hDay = CreateWindowW(L"COMBOBOX", NULL,
                                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                comboStartX, 60, comboBoxWidth, 200, hwnd, (HMENU)ID_DAY, NULL, NULL);
            FillComboBox(hDay, 0, 99);
            SendMessage(hDay, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hLabelDay = CreateWindowW(L"STATIC", L"天",
                                          WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          comboStartX + comboBoxWidth, 65, labelWidth, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelDay, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hHour = CreateWindowW(L"COMBOBOX", NULL,
                                 WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 comboStartX + comboBoxWidth + labelWidth + 10, 60, comboBoxWidth, 200, hwnd, (HMENU)ID_HOUR, NULL, NULL);
            FillComboBox(hHour, 0, 23);
            SendMessage(hHour, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hLabelHour = CreateWindowW(L"STATIC", L"时",
                                           WS_VISIBLE | WS_CHILD | SS_LEFT,
                                           comboStartX + 2 * comboBoxWidth + labelWidth + 10, 65, labelWidth, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelHour, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hMinute = CreateWindowW(L"COMBOBOX", NULL,
                                   WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   comboStartX + 2 * (comboBoxWidth + labelWidth) + 20, 60, comboBoxWidth, 200, hwnd, (HMENU)ID_MINUTE, NULL, NULL);
            FillComboBox(hMinute, 0, 59);
            SendMessage(hMinute, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hLabelMinute = CreateWindowW(L"STATIC", L"分",
                                             WS_VISIBLE | WS_CHILD | SS_LEFT,
                                             comboStartX + 3 * comboBoxWidth + 2 * labelWidth + 20, 65, labelWidth, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelMinute, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建倒计时显示标签
            hCountdownLabel = CreateWindowW(L"STATIC", L"",
                                           WS_VISIBLE | WS_CHILD | SS_CENTER,
                                           50, 100, 400, 40, hwnd, (HMENU)ID_COUNTDOWN, NULL, NULL);
            SendMessage(hCountdownLabel, WM_SETFONT, (WPARAM)hCountdownFont, TRUE);
            ShowWindow(hCountdownLabel, SW_HIDE);

            int buttonWidth = 100;
            int buttonSpacing = 10;
            int totalButtonWidth = 4 * buttonWidth + 3 * buttonSpacing;
            int buttonStartX = (windowWidth - totalButtonWidth) / 2;

            // 创建操作按钮
            HWND hButtonSchedule = CreateWindowW(L"BUTTON", L"确定",
                                               WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                               buttonStartX, 150, buttonWidth, 30, hwnd, (HMENU)ID_SCHEDULE, NULL, NULL);
            SendMessage(hButtonSchedule, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hButtonCancel = CreateWindowW(L"BUTTON", L"清除",
                                             WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                             buttonStartX + buttonWidth + buttonSpacing, 150, buttonWidth, 30, hwnd, (HMENU)ID_CANCEL, NULL, NULL);
            SendMessage(hButtonCancel, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hButtonMinimize = CreateWindowW(L"BUTTON", L"最小化",
                                               WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                               buttonStartX + 2 * (buttonWidth + buttonSpacing), 150, buttonWidth, 30, hwnd, (HMENU)ID_MINIMIZE, NULL, NULL);
            SendMessage(hButtonMinimize, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hButtonExit = CreateWindowW(L"BUTTON", L"退出程序",
                                           WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                           buttonStartX + 3 * (buttonWidth + buttonSpacing), 150, buttonWidth, 30, hwnd, (HMENU)ID_EXIT, NULL, NULL);
            SendMessage(hButtonExit, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hSepLine = CreateWindowW(L"Static", L"", SS_ETCHEDHORZ | WS_CHILD | WS_VISIBLE, 5, 190, 490, 10, hwnd, NULL, NULL, NULL);
            SendMessage(hSepLine, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            // 创建超链接控件
            HWND hURL = CreateWindowW(L"Static", L"软件主页：YINGMING006.GITHUB.IO/SHUTDOWNTOOL", WS_VISIBLE | WS_CHILD | SS_NOTIFY | SS_CENTER, 70, 196, 400, 30, hwnd, (HMENU)ID_URL, NULL, NULL);
            SendMessage(hURL, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);
            break;
        }
        case WM_COMMAND: {
            if (HIWORD(wParam) == BN_CLICKED) {  // 处理按钮点击事件
                HWND hButton = (HWND)lParam;
                EnableWindow(hButton, FALSE);    // 临时禁用按钮防止重复点击
                switch (LOWORD(wParam)) {
                    case ID_SCHEDULE: {          // 确定按钮
                        int day = SendMessage(hDay, CB_GETCURSEL, 0, 0);
                        int hour = SendMessage(hHour, CB_GETCURSEL, 0, 0);
                        int minute = SendMessage(hMinute, CB_GETCURSEL, 0, 0);

                        int operation = ID_SHUTDOWN;
                        if (SendMessage(hRestartRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                            operation = ID_RESTART;
                        } else if (SendMessage(hSleepRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                            operation = ID_SLEEP;
                        }
                        ScheduleShutdown(hwnd, day, hour, minute, operation);
                        break;
                    }
                    case ID_CANCEL:              // 清除按钮
                        CancelShutdownTool(hwnd);
                        break;
                    case ID_MINIMIZE:           // 最小化按钮
                        ShowWindow(hwnd, SW_MINIMIZE);
                        break;
                    case ID_EXIT:               // 退出按钮
                        CancelShutdownTool(hwnd);
                        DestroyWindow(hwnd);
                        break;
                    case ID_URL:                // 点击超链接
                        ShellExecute(NULL, L"open", L"https://yingming006.github.io/ShutdownTool", NULL, NULL, SW_SHOWNORMAL);
                        break;
                }
                EnableWindow(hButton, TRUE);    // 恢复按钮可用状态
            }
            break;
        }
        case WM_TIMER: {                       // 计时器消息
            if (wParam == ID_TIMER && remainingSeconds > 0) {
                remainingSeconds--;
                UpdateCountdown(hwnd);
                if (remainingSeconds == 0) {
                    KillTimer(hwnd, ID_TIMER);
                    isTimerRunning = FALSE;
                }
            }
            break;
        }
        case WM_MOUSEWHEEL: {                 // 鼠标滚轮事件
            HWND hFocus = GetFocus();
            if (hFocus == hDay || hFocus == hHour || hFocus == hMinute) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int currentIndex = SendMessage(hFocus, CB_GETCURSEL, 0, 0);
                int newIndex = currentIndex - (delta / WHEEL_DELTA);
                int count = SendMessage(hFocus, CB_GETCOUNT, 0, 0);
                newIndex = max(0, min(newIndex, count - 1));
                SendMessage(hFocus, CB_SETCURSEL, newIndex, 0);
            }
            break;
        }
        case WM_ERASEBKGND: {                // 绘制背景
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, hBackgroundBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC:             // 控件背景颜色
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(240, 240, 240));
            SetTextColor(hdc, RGB(0, 0, 0));
            return (LRESULT)hBackgroundBrush;
        }
        case WM_SIZE: {                     // 窗口大小改变
            if (wParam == SIZE_MINIMIZED) { // 最小化到托盘
                ShowWindow(hwnd, SW_HIDE);
                nid.cbSize = sizeof(NOTIFYICONDATA);
                nid.hWnd = hwnd;
                nid.uID = ID_TRAYICON;
                nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                nid.uCallbackMessage = WM_TRAYICON;
                nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
                wcscpy(nid.szTip, L"定时关机工具");
                Shell_NotifyIcon(NIM_ADD, &nid);
            }
            break;
        }
        case WM_TRAYICON: {                // 托盘图标消息
            if (lParam == WM_LBUTTONDOWN) { // 左键点击恢复窗口
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                Shell_NotifyIcon(NIM_DELETE, &nid);
            }
            break;
        }
        case WM_DESTROY: {                // 窗口销毁
            Shell_NotifyIcon(NIM_DELETE, &nid);
            CleanupResources();
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
    if (!InitResources()) {
        MessageBoxW(NULL, L"资源初始化失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ShutdownToolClass";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassW(&wc);

    // 计算窗口居中位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 500;  // 减小窗口宽度
    int windowHeight = 250;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    // 创建主窗口
    HWND hwnd = CreateWindowExW(0, L"ShutdownToolClass", L"定时关机工具",
                               WS_OVERLAPPED | WS_CAPTION,
                               x, y, windowWidth, windowHeight,
                               NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        CleanupResources();
        return 1;
    }

    SendMessage(hwnd, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);
    ShowWindow(hwnd, nCmdShow);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}