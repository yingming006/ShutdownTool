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

#define REG_PATH L"Software\\ShutdownTool"
#define REG_NAME L"ShutdownTime"

BOOL RunProcess(LPCWSTR command);
void ShowErrorMessage(HWND hwnd, const wchar_t* message);
void ScheduleShutdown(HWND hwnd, int days, int hours, int minutes, int operation);
void CancelShutdownTool(HWND hwnd);
void FillComboBox(HWND hComboBox, int start, int end);
void UpdateCountdown(HWND hwnd);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL IsShutdownScheduled(void);

void SaveShutdownTime(SYSTEMTIME targetTime);
BOOL LoadShutdownTime(SYSTEMTIME* targetTime);
void ClearShutdownTime(void);
void CalculateRemainingTime(const SYSTEMTIME* targetTime);

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

    remainingSeconds = (int)((target.QuadPart - current.QuadPart) / 10000000LL);
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
        ShowErrorMessage(hwnd, L"请设置大于0的时间！");
        return;
    }

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

void CancelShutdownTool(HWND hwnd) {
    if (!isTimerRunning) return;

    if (RunProcess(L"shutdown -a")) {
        remainingSeconds = 0;
        KillTimer(hwnd, ID_TIMER);
        ShowWindow(hCountdownLabel, SW_HIDE);
        isTimerRunning = FALSE;
        ClearShutdownTime();
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
            int radioWidth = 80;
            int radioSpacing = 10;
            int totalRadioWidth = 3 * radioWidth + 2 * radioSpacing;
            int startX = (500 - totalRadioWidth) / 2;

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

            SendMessage(hShutdownRadio, BM_SETCHECK, BST_CHECKED, 0);

            hDay = CreateWindowW(L"COMBOBOX", NULL,
                                WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                100, 60, 60, 200, hwnd, (HMENU)ID_DAY, NULL, NULL);
            FillComboBox(hDay, 0, 99);
            SendMessage(hDay, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hLabelDay = CreateWindowW(L"STATIC", L"天",
                                          WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          170, 65, 30, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelDay, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hHour = CreateWindowW(L"COMBOBOX", NULL,
                                 WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 210, 60, 60, 200, hwnd, (HMENU)ID_HOUR, NULL, NULL);
            FillComboBox(hHour, 0, 23);
            SendMessage(hHour, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hLabelHour = CreateWindowW(L"STATIC", L"时",
                                           WS_VISIBLE | WS_CHILD | SS_LEFT,
                                           280, 65, 30, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelHour, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            hMinute = CreateWindowW(L"COMBOBOX", NULL,
                                   WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   320, 60, 60, 200, hwnd, (HMENU)ID_MINUTE, NULL, NULL);
            FillComboBox(hMinute, 0, 59);
            SendMessage(hMinute, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hLabelMinute = CreateWindowW(L"STATIC", L"分",
                                             WS_VISIBLE | WS_CHILD | SS_LEFT,
                                             390, 65, 30, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabelMinute, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

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

            HWND hButtonSchedule = CreateWindowW(L"BUTTON", L"确定",
                                               WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                               150, 150, 100, 30, hwnd, (HMENU)ID_SCHEDULE, NULL, NULL);
            SendMessage(hButtonSchedule, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            HWND hButtonCancel = CreateWindowW(L"BUTTON", L"清除",
                                             WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                             260, 150, 100, 30, hwnd, (HMENU)ID_CANCEL, NULL, NULL);
            SendMessage(hButtonCancel, WM_SETFONT, (WPARAM)hGlobalFont, TRUE);

            SYSTEMTIME targetTime;
            if (LoadShutdownTime(&targetTime)) {
                CalculateRemainingTime(&targetTime);
                if (remainingSeconds > 0) {
                    if (IsShutdownScheduled()) {
                        isTimerRunning = TRUE;
                        SetTimer(hwnd, ID_TIMER, 1000, NULL);
                        UpdateCountdown(hwnd);
                    } else {
                        ClearShutdownTime();
                        remainingSeconds = 0;
                    }
                } else {
                    ClearShutdownTime();
                }
            }
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
                    ClearShutdownTime();
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
        case WM_DESTROY: {
            if (hGlobalFont) DeleteObject(hGlobalFont);
            if (hCountdownFont) DeleteObject(hCountdownFont);
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            PostQuitMessage(0);
            break;
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
    int windowWidth = 500;
    int windowHeight = 250;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExW(0, L"ShutdownToolClass", L"定时关机工具",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
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