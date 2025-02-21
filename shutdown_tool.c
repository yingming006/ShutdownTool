#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/subsystem:windows /entry:wWinMainCRTStartup")

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
#define ID_MINIMIZE 1012
#define ID_EXIT 1013

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1

HWND hDay, hHour, hMinute;
HWND hShutdownRadio, hRestartRadio, hSleepRadio;
HWND hCountdownLabel;
HFONT hGlobalFont;
HBRUSH hBackgroundBrush;
int remainingSeconds = 0;
HFONT hCountdownFont = NULL;
BOOL isTimerRunning = FALSE;

NOTIFYICONDATA nid = {0};

BOOL RunProcess(LPCWSTR command);
void ShowErrorMessage(HWND hwnd, const wchar_t* message);
void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation);
void CancelShutdownTool(HWND hwnd);
void FillComboBox(HWND hComboBox, int start, int end);
void UpdateCountdown(HWND hwnd);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL IsShutdownScheduled(void);

BOOL RunProcess(LPCWSTR command) {
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

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

void ShowErrorMessage(HWND hwnd, const wchar_t* message) {
    MessageBoxW(hwnd, message, L"错误", MB_OK | MB_ICONERROR);
}

BOOL IsShutdownScheduled(void) {
    return RunProcess(L"shutdown -a");
}

void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation) {
    if (isTimerRunning) {
        if (MessageBoxW(hwnd, L"已有定时任务在运行，是否重新设置？", 
                       L"提示", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
        CancelShutdownTool(hwnd);
    }

    remainingSeconds = days * 86400 + hours * 3600 + minutes * 60;
    if (remainingSeconds <= 0) {
        return;  // 直接返回，不弹出提示框
    }

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
        return;
    }

    SetTimer(hwnd, ID_TIMER, 1000, NULL);
    isTimerRunning = TRUE;
}

void CancelShutdownTool(HWND hwnd) {
    if (!isTimerRunning) return;

    if (RunProcess(L"shutdown -a")) {
        remainingSeconds = 0;
        KillTimer(hwnd, ID_TIMER);
        ShowWindow(hCountdownLabel, SW_HIDE);
        isTimerRunning = FALSE;
    }
}

void FillComboBox(HWND hComboBox, int start, int end) {
    for (int i = start; i <= end; i++) {
        wchar_t buffer[10];
        swprintf(buffer, 10, L"%02d", i);
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)buffer);
    }
    SendMessage(hComboBox, CB_SETCURSEL, 0, 0);
}

void UpdateCountdown(HWND hwnd) {
    if (remainingSeconds <= 0) {
        ShowWindow(hCountdownLabel, SW_HIDE);
        return;
    }

    int days = remainingSeconds / 86400;
    int hours = (remainingSeconds % 86400) / 3600;
    int minutes = (remainingSeconds % 3600) / 60;
    int seconds = remainingSeconds % 60;

    wchar_t countdownText[100];
    swprintf(countdownText, 100, L"倒计时: %02d 天 %02d 时 %02d 分 %02d 秒", days, hours, minutes, seconds);
    SetWindowTextW(hCountdownLabel, countdownText);
    ShowWindow(hCountdownLabel, SW_SHOW);
    InvalidateRect(hCountdownLabel, NULL, TRUE);
    UpdateWindow(hCountdownLabel);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            int windowWidth = 650;
            int radioWidth = 80;
            int radioSpacing = 10;
            int totalRadioWidth = 3 * radioWidth + 2 * radioSpacing;
            int radioStartX = (windowWidth - totalRadioWidth) / 2;

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

            SendMessage(hShutdownRadio, BM_SETCHECK, BST_CHECKED, 0);

            int comboBoxWidth = 60;
            int labelWidth = 30;
            int totalComboWidth = 3 * comboBoxWidth + 2 * labelWidth + 20;
            int comboStartX = (windowWidth - totalComboWidth) / 2;

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

            hCountdownLabel = CreateWindowW(L"STATIC", L"",
                                           WS_VISIBLE | WS_CHILD | SS_CENTER,
                                           50, 100, 550, 40, hwnd, (HMENU)ID_COUNTDOWN, NULL, NULL);
            hCountdownFont = CreateFont(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                         DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
            if (hCountdownFont) {
                SendMessage(hCountdownLabel, WM_SETFONT, (WPARAM)hCountdownFont, TRUE);
            }
            ShowWindow(hCountdownLabel, SW_HIDE);

            int buttonWidth = 100;
            int buttonSpacing = 10;
            int totalButtonWidth = 4 * buttonWidth + 3 * buttonSpacing;
            int buttonStartX = (windowWidth - totalButtonWidth) / 2;

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

            break;
        }
        case WM_COMMAND: {
            if (HIWORD(wParam) == BN_CLICKED) {
                HWND hButton = (HWND)lParam;
                EnableWindow(hButton, FALSE);
                if (LOWORD(wParam) == ID_SCHEDULE) {
                    int day = SendMessage(hDay, CB_GETCURSEL, 0, 0);
                    int hour = SendMessage(hHour, CB_GETCURSEL, 0, 0);
                    int minute = SendMessage(hMinute, CB_GETCURSEL, 0, 0);

                    int operation = 0;
                    if (SendMessage(hShutdownRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        operation = ID_SHUTDOWN;
                    } else if (SendMessage(hRestartRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        operation = ID_RESTART;
                    } else if (SendMessage(hSleepRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        operation = ID_SLEEP;
                    }

                    ScheduleShutdown(hwnd, day, hour, minute, operation);
                    UpdateCountdown(hwnd);
                } else if (LOWORD(wParam) == ID_CANCEL) {
                    CancelShutdownTool(hwnd);
                } else if (LOWORD(wParam) == ID_MINIMIZE) {
                    ShowWindow(hwnd, SW_MINIMIZE);
                } else if (LOWORD(wParam) == ID_EXIT) {
                    CancelShutdownTool(hwnd);
                    DestroyWindow(hwnd);
                }
                EnableWindow(hButton, TRUE);
            }
            break;
        }
        case WM_TIMER: {
            if (wParam == ID_TIMER) {
                if (remainingSeconds > 0) {
                    remainingSeconds--;
                    UpdateCountdown(hwnd);
                } else {
                    KillTimer(hwnd, ID_TIMER);
                    ShowWindow(hCountdownLabel, SW_HIDE);
                    isTimerRunning = FALSE;
                }
            }
            break;
        }
        case WM_MOUSEWHEEL: {
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
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, hBackgroundBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(240, 240, 240));
            SetTextColor(hdc, RGB(0, 0, 0));
            return (LRESULT)hBackgroundBrush;
        }
        case WM_CTLCOLORBTN: {
            return (LRESULT)hBackgroundBrush;
        }
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
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
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDOWN) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                Shell_NotifyIcon(NIM_DELETE, &nid);
            }
            break;
        }
        case WM_DESTROY: {
            Shell_NotifyIcon(NIM_DELETE, &nid);
            if (hGlobalFont) DeleteObject(hGlobalFont);
            if (hCountdownFont) DeleteObject(hCountdownFont);
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    hGlobalFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ShutdownToolClass";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 650;
    int windowHeight = 250;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExW(0, L"ShutdownToolClass", L"定时关机工具",
                               WS_OVERLAPPED | WS_CAPTION,
                               x, y, windowWidth, windowHeight,
                               NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        return 0;
    }

    SendMessage(hwnd, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}