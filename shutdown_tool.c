#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <shellapi.h>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define ID_ICON 1
#define WM_TRAY_ICON_MSG (WM_APP + 1)
#define ID_MENU_ABOUT 201
#define ID_MENU_EXIT 202

#define ID_RADIO_SHUTDOWN 101
#define ID_RADIO_REBOOT 102
#define ID_RADIO_SLEEP 103
#define ID_COMBO_DAY 104
#define ID_COMBO_HOUR 105
#define ID_COMBO_MINUTE 106
#define ID_BUTTON_START 107
#define ID_BUTTON_CLEAR 108
#define ID_LABEL_COUNTDOWN 111
#define ID_TIMER 1

#define OP_SHUTDOWN 0
#define OP_REBOOT 1
#define OP_SLEEP 2

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PopulateComboBoxes();
void UpdateCountdownDisplay();
void StartCountdown(HWND hwnd);
void ClearSettings(HWND hwnd);
void SetControlsEnabled(BOOL bEnable);
void ExecuteShutdownAction();
BOOL CALLBACK SetChildFont(HWND hwnd, LPARAM lParam);
void CreateTrayIcon(HWND hwnd, HICON hIcon);
void ShowTrayMenu(HWND hwnd);

HWND hRadioShutdown, hRadioReboot, hRadioSleep;
HWND hComboDay, hComboHour, hComboMinute;
HWND hBtnStart, hBtnClear;
HWND hLabelCountdown;
HFONT g_hFontUI, g_hFontCountdown;
HBRUSH g_hBrushBkg;
NOTIFYICONDATAW nid;

UINT_PTR g_TimerID = 0;
long long g_TotalSeconds = 0;
BOOL g_isTimerRunning = FALSE;
BOOL g_isPaused = FALSE;
int g_selectedOperation = OP_SHUTDOWN;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"ShutdownTimerClass";
    HWND hExistingWnd;

    hExistingWnd = FindWindowW(CLASS_NAME, NULL);
    if (hExistingWnd)
    {
        if (IsIconic(hExistingWnd))
        {
            ShowWindow(hExistingWnd, SW_RESTORE);
        }
        SetForegroundWindow(hExistingWnd);
        
        return FALSE;
    }

    const int WINDOW_WIDTH = 430, WINDOW_HEIGHT = 360;
    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(ID_ICON));
    g_hBrushBkg = CreateSolidBrush(RGB(240, 240, 240));

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBrushBkg;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassExW(&wc))
        return 0;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN), screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int centerX = (screenWidth - WINDOW_WIDTH) / 2, centerY = (screenHeight - WINDOW_HEIGHT) / 2;

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"定时关机", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                centerX, centerY, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);
    if (hwnd == NULL)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        g_hFontUI = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_hFontCountdown = CreateFontW(35, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        CreateControls(hwnd);
        CreateTrayIcon(hwnd, (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(ID_ICON), IMAGE_ICON, 16, 16, 0));
        break;
    case WM_TRAY_ICON_MSG:
        switch (lParam)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            break;
        case WM_RBUTTONDOWN:
            ShowTrayMenu(hwnd);
            break;
        }
        break;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        SetTextColor(hdcStatic, RGB(0, 0, 0));
        return (LRESULT)g_hBrushBkg;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_RADIO_SHUTDOWN:
            g_selectedOperation = OP_SHUTDOWN;
            break;
        case ID_RADIO_REBOOT:
            g_selectedOperation = OP_REBOOT;
            break;
        case ID_RADIO_SLEEP:
            g_selectedOperation = OP_SLEEP;
            break;
        case ID_BUTTON_START:
            if (!g_isTimerRunning)
                StartCountdown(hwnd);
            else
            {
                g_isPaused = !g_isPaused;
                SetWindowTextW(hBtnStart, g_isPaused ? L"继续" : L"暂停");
            }
            break;
        case ID_BUTTON_CLEAR:
            ClearSettings(hwnd);
            break;
        case ID_MENU_ABOUT:
            ShellExecuteW(hwnd, L"open", L"https://yingming006.github.io/ShutdownTool/", NULL, NULL, SW_SHOW);
            break;
        case ID_MENU_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        break;
    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE)
        {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            ShowWindow(hwnd, SW_HIDE);
        }
        break;
    case WM_TIMER:
        if (wParam == g_TimerID && !g_isPaused)
        {
            if (--g_TotalSeconds < 0)
                g_TotalSeconds = 0;
            UpdateCountdownDisplay();
            if (g_TotalSeconds == 0)
            {
                KillTimer(hwnd, g_TimerID);
                g_TimerID = 0;
                ExecuteShutdownAction();
                DestroyWindow(hwnd);
            }
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        if (g_TimerID != 0)
            KillTimer(hwnd, g_TimerID);
        DeleteObject(g_hFontUI);
        DeleteObject(g_hFontCountdown);
        DeleteObject(g_hBrushBkg);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd)
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    const int MARGIN = 15, GROUP_V_GAP = 12, GROUP_H_PADDING = 20, CONTROL_V_PADDING = 25;
    const int COUNTDOWN_TOP_MARGIN = 15, BUTTON_AREA_TOP_MARGIN = 20;
    int yPos = MARGIN, groupWidth = 430 - 2 * MARGIN;

    HWND hGroupOp = CreateWindowExW(0, L"BUTTON", L" 操作选择 ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, MARGIN, yPos, groupWidth, 70, hwnd, NULL, hInstance, NULL);
    int radioY = yPos + CONTROL_V_PADDING + 5, radioSlotWidth = (groupWidth - 2 * GROUP_H_PADDING) / 3;
    hRadioShutdown = CreateWindowExW(0, L"BUTTON", L"关机", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, MARGIN + GROUP_H_PADDING, radioY, 90, 30, hwnd, (HMENU)ID_RADIO_SHUTDOWN, hInstance, NULL);
    hRadioReboot = CreateWindowExW(0, L"BUTTON", L"重启", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, MARGIN + GROUP_H_PADDING + radioSlotWidth, radioY, 90, 30, hwnd, (HMENU)ID_RADIO_REBOOT, hInstance, NULL);
    hRadioSleep = CreateWindowExW(0, L"BUTTON", L"睡眠", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, MARGIN + GROUP_H_PADDING + 2 * radioSlotWidth, radioY, 90, 30, hwnd, (HMENU)ID_RADIO_SLEEP, hInstance, NULL);
    SendMessage(hRadioShutdown, BM_SETCHECK, BST_CHECKED, 0);
    yPos += 70 + GROUP_V_GAP;

    HWND hGroupTime = CreateWindowExW(0, L"BUTTON", L" 时间设置 ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, MARGIN, yPos, groupWidth, 75, hwnd, NULL, hInstance, NULL);
    int timeY = yPos + CONTROL_V_PADDING + 5, comboWidth = 80, labelWidth = 35, timeSlotWidth = (groupWidth - 2 * GROUP_H_PADDING) / 3, timeX_Start = MARGIN + GROUP_H_PADDING;
    hComboDay = CreateWindowExW(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_VISIBLE, timeX_Start, timeY, comboWidth, 300, hwnd, (HMENU)ID_COMBO_DAY, hInstance, NULL);
    CreateWindowExW(0, L"STATIC", L"天", WS_CHILD | WS_VISIBLE | SS_LEFT, timeX_Start + comboWidth + 8, timeY + 4, labelWidth, 30, hwnd, NULL, hInstance, NULL);
    hComboHour = CreateWindowExW(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_VISIBLE, timeX_Start + timeSlotWidth, timeY, comboWidth, 300, hwnd, (HMENU)ID_COMBO_HOUR, hInstance, NULL);
    CreateWindowExW(0, L"STATIC", L"时", WS_CHILD | WS_VISIBLE | SS_LEFT, timeX_Start + timeSlotWidth + comboWidth + 8, timeY + 4, labelWidth, 30, hwnd, NULL, hInstance, NULL);
    hComboMinute = CreateWindowExW(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_VISIBLE, timeX_Start + 2 * timeSlotWidth, timeY, comboWidth, 300, hwnd, (HMENU)ID_COMBO_MINUTE, hInstance, NULL);
    CreateWindowExW(0, L"STATIC", L"分", WS_CHILD | WS_VISIBLE | SS_LEFT, timeX_Start + 2 * timeSlotWidth + comboWidth + 8, timeY + 4, labelWidth, 30, hwnd, NULL, hInstance, NULL);
    PopulateComboBoxes();
    yPos += 75 + COUNTDOWN_TOP_MARGIN;

    hLabelCountdown = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, MARGIN, yPos, groupWidth, 45, hwnd, NULL, hInstance, NULL);
    yPos += 45 + BUTTON_AREA_TOP_MARGIN;

    int btnWidth = 120, btnHeight = 40;
    int totalBtnWidth = 2 * btnWidth + 20;
    int btnX_Start = (430 - totalBtnWidth) / 2;
    hBtnStart = CreateWindowExW(0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, btnX_Start, yPos, btnWidth, btnHeight, hwnd, (HMENU)ID_BUTTON_START, hInstance, NULL);
    hBtnClear = CreateWindowExW(0, L"BUTTON", L"清除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, btnX_Start + btnWidth + 20, yPos, btnWidth, btnHeight, hwnd, (HMENU)ID_BUTTON_CLEAR, hInstance, NULL);

    EnumChildWindows(hwnd, SetChildFont, (LPARAM)g_hFontUI);
    SendMessage(hLabelCountdown, WM_SETFONT, (WPARAM)g_hFontCountdown, TRUE);
}

void CreateTrayIcon(HWND hwnd, HICON hIcon)
{
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = ID_ICON;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_ICON_MSG;
    nid.hIcon = hIcon;
    wcscpy(nid.szTip, L"定时关机");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void ShowTrayMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_MENU_ABOUT, L"关于");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_EXIT, L"退出");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

BOOL CALLBACK SetChildFont(HWND hwnd, LPARAM lParam)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}
void PopulateComboBoxes()
{
    wchar_t b[5];
    for (int i = 0; i < 100; ++i)
    {
        wsprintfW(b, L"%02d", i);
        SendMessage(hComboDay, CB_ADDSTRING, 0, (LPARAM)b);
        SendMessage(hComboHour, CB_ADDSTRING, 0, (LPARAM)b);
        SendMessage(hComboMinute, CB_ADDSTRING, 0, (LPARAM)b);
    }
    SendMessage(hComboDay, CB_SETCURSEL, 0, 0);
    SendMessage(hComboHour, CB_SETCURSEL, 0, 0);
    SendMessage(hComboMinute, CB_SETCURSEL, 0, 0);
}
void UpdateCountdownDisplay()
{
    if (g_TotalSeconds <= 0 && !g_isTimerRunning)
    {
        SetWindowTextW(hLabelCountdown, L"");
        return;
    }
    long long r = g_TotalSeconds;
    int d = r / 86400;
    r %= 86400;
    int h = r / 3600;
    r %= 3600;
    int m = r / 60;
    int s = r % 60;
    wchar_t ts[100] = {0}, t[20];
    if (d > 0)
    {
        wsprintfW(t, L"%d天 ", d);
        wcscat(ts, t);
    }
    if (h > 0 || d > 0)
    {
        wsprintfW(t, L"%02d时 ", h);
        wcscat(ts, t);
    }
    if (m > 0 || h > 0 || d > 0)
    {
        wsprintfW(t, L"%02d分 ", m);
        wcscat(ts, t);
    }
    wsprintfW(t, L"%02d秒", s);
    wcscat(ts, t);
    const wchar_t *os;
    switch (g_selectedOperation)
    {
    case OP_REBOOT:
        os = L"重启";
        break;
    case OP_SLEEP:
        os = L"睡眠";
        break;
    default:
        os = L"关机";
        break;
    }
    wchar_t b[200];
    wsprintfW(b, L"将在 %s 后 %s", ts, os);
    SetWindowTextW(hLabelCountdown, b);
}
void StartCountdown(HWND hwnd)
{
    g_TotalSeconds = (long long)SendMessage(hComboDay, CB_GETCURSEL, 0, 0) * 86400 + (long long)SendMessage(hComboHour, CB_GETCURSEL, 0, 0) * 3600 + (long long)SendMessage(hComboMinute, CB_GETCURSEL, 0, 0) * 60;
    if (g_TotalSeconds <= 0)
    {
        MessageBoxW(hwnd, L"请设置一个有效的倒计时长。", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    g_isTimerRunning = TRUE;
    g_isPaused = FALSE;
    g_TimerID = SetTimer(hwnd, ID_TIMER, 1000, NULL);
    SetWindowTextW(hBtnStart, L"暂停");
    SetControlsEnabled(FALSE);
    UpdateCountdownDisplay();
}
void ClearSettings(HWND hwnd)
{
    if (g_TimerID != 0)
    {
        KillTimer(hwnd, g_TimerID);
        g_TimerID = 0;
    }
    g_isTimerRunning = FALSE;
    g_isPaused = FALSE;
    g_TotalSeconds = 0;
    SetWindowTextW(hBtnStart, L"确定");
    SendMessage(hComboDay, CB_SETCURSEL, 0, 0);
    SendMessage(hComboHour, CB_SETCURSEL, 0, 0);
    SendMessage(hComboMinute, CB_SETCURSEL, 0, 0);
    SendMessage(hRadioShutdown, BM_SETCHECK, BST_CHECKED, 0);
    g_selectedOperation = OP_SHUTDOWN;
    SetControlsEnabled(TRUE);
    UpdateCountdownDisplay();
}
void SetControlsEnabled(BOOL bEnable)
{
    EnableWindow(hRadioShutdown, bEnable);
    EnableWindow(hRadioReboot, bEnable);
    EnableWindow(hRadioSleep, bEnable);
    EnableWindow(hComboDay, bEnable);
    EnableWindow(hComboHour, bEnable);
    EnableWindow(hComboMinute, bEnable);
}
void ExecuteShutdownAction()
{
    switch (g_selectedOperation)
    {
    case OP_SHUTDOWN:
        system("shutdown /s /t 0");
        break;
    case OP_REBOOT:
        system("shutdown /r /t 0");
        break;
    case OP_SLEEP:
        system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
        break;
    }
}