#include <windows.h>
#include <shellapi.h>

// 强制使用 Windows 风格控件
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
#define ID_CHECK_NOTIFY 109
#define ID_LABEL_COUNTDOWN 111
#define ID_TIMER 1

#define OP_SHUTDOWN 0
#define OP_REBOOT 1
#define OP_SLEEP 2

#define NOTIFICATION_TIME_SECONDS 55

// 全局变量
HWND hRadioShutdown, hRadioReboot, hRadioSleep;
HWND hComboDay, hComboHour, hComboMinute;
HWND hBtnStart, hBtnClear;
HWND hCheckNotify;
HWND hLabelCountdown;
HFONT g_hFontUI, g_hFontCountdown;
HBRUSH g_hBrushBkg;
NOTIFYICONDATAW nid;

UINT_PTR g_TimerID = 0;
// 【优化】改为 int，避免 32 位下需要 __divmoddi4 库函数
int g_TotalSeconds = 0;
BOOL g_isTimerRunning = FALSE;
BOOL g_isPaused = FALSE;
BOOL g_bNotifyBeforeEnd = FALSE;
BOOL g_bNotificationShown = FALSE;
int g_selectedOperation = OP_SHUTDOWN;

// 函数声明
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

// 自定义入口点
void __cdecl WinMainCRTStartup()
{
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    
    const wchar_t CLASS_NAME[] = L"ShutdownTimerClass";
    HWND hExistingWnd = FindWindowW(CLASS_NAME, NULL);
    if (hExistingWnd)
    {
        ShowWindow(hExistingWnd, SW_RESTORE);
        SetForegroundWindow(hExistingWnd);
        ExitProcess(0);
    }

    const int WINDOW_WIDTH = 430, WINDOW_HEIGHT = 280;
    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(ID_ICON));
    g_hBrushBkg = CreateSolidBrush(RGB(240, 240, 240));

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = g_hBrushBkg;
    wc.lpszClassName = CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) ExitProcess(0);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int centerX = (screenWidth - WINDOW_WIDTH) / 2;
    int centerY = (screenHeight - WINDOW_HEIGHT) / 2;

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"定时关机", 
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                centerX, centerY, WINDOW_WIDTH, WINDOW_HEIGHT, 
                                NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) ExitProcess(0);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ExitProcess((UINT)msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        g_hFontUI = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_hFontCountdown = CreateFontW(35, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        CreateControls(hwnd);
        CreateTrayIcon(hwnd, (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(ID_ICON), IMAGE_ICON, 16, 16, 0));
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
        HWND hwndStatic = (HWND)lParam;
        SetTextColor(hdcStatic, RGB(0, 0, 0));
        if (hwndStatic == hLabelCountdown)
        {
            SetBkColor(hdcStatic, RGB(240, 240, 240));
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        else
        {
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)g_hBrushBkg;
        }
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_RADIO_SHUTDOWN: g_selectedOperation = OP_SHUTDOWN; break;
        case ID_RADIO_REBOOT: g_selectedOperation = OP_REBOOT; break;
        case ID_RADIO_SLEEP: g_selectedOperation = OP_SLEEP; break;
        case ID_BUTTON_START:
            if (!g_isTimerRunning) StartCountdown(hwnd);
            else
            {
                g_isPaused = !g_isPaused;
                SetWindowTextW(hBtnStart, g_isPaused ? L"继续" : L"暂停");
            }
            break;
        case ID_BUTTON_CLEAR: ClearSettings(hwnd); break;
        case ID_CHECK_NOTIFY:
            g_bNotifyBeforeEnd = (SendMessageW(hCheckNotify, BM_GETCHECK, 0, 0) == BST_CHECKED);
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
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) ShowWindow(hwnd, SW_HIDE);
        break;

    case WM_TIMER:
        if (wParam == g_TimerID && !g_isPaused)
        {
            if (g_bNotifyBeforeEnd && !g_bNotificationShown && g_TotalSeconds == NOTIFICATION_TIME_SECONDS)
            {
                g_bNotificationShown = TRUE;
                const wchar_t *opStr = (g_selectedOperation == OP_REBOOT) ? L"重启" : 
                                      (g_selectedOperation == OP_SLEEP) ? L"睡眠" : L"关机";
                wchar_t msg[256];
                wsprintfW(msg, L"程序将在 %d 秒后执行【%s】操作。\n请及时保存您的工作！", NOTIFICATION_TIME_SECONDS, opStr);
                MessageBoxW(hwnd, msg, L"操作提醒", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            }

            if (--g_TotalSeconds < 0) g_TotalSeconds = 0;
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
        if (g_TimerID != 0) KillTimer(hwnd, g_TimerID);
        DeleteObject(g_hFontUI);
        DeleteObject(g_hFontCountdown);
        DeleteObject(g_hBrushBkg);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd)
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    const int WINDOW_W = 430;
    int yPos = 20;

    const int radioWidth = 70, radioGap = 30;
    int radioX_Start = (WINDOW_W - (3 * radioWidth + 2 * radioGap)) / 2;
    
    hRadioShutdown = CreateWindowExW(0, L"BUTTON", L"关机", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, radioX_Start, yPos, radioWidth, 30, hwnd, (HMENU)ID_RADIO_SHUTDOWN, hInstance, NULL);
    hRadioReboot = CreateWindowExW(0, L"BUTTON", L"重启", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, radioX_Start + radioWidth + radioGap, yPos, radioWidth, 30, hwnd, (HMENU)ID_RADIO_REBOOT, hInstance, NULL);
    hRadioSleep = CreateWindowExW(0, L"BUTTON", L"睡眠", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, radioX_Start + 2 * (radioWidth + radioGap), yPos, radioWidth, 30, hwnd, (HMENU)ID_RADIO_SLEEP, hInstance, NULL);
    SendMessageW(hRadioShutdown, BM_SETCHECK, BST_CHECKED, 0);
    yPos += 50;

    const int comboWidth = 65, comboLabelGap = 5, labelWidth = 25, timeUnitGap = 20;
    const int timeUnitWidth = comboWidth + comboLabelGap + labelWidth;
    int timeX_Start = (WINDOW_W - (3 * timeUnitWidth + 2 * timeUnitGap)) / 2;
    
    hComboDay = CreateWindowExW(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_VISIBLE, timeX_Start, yPos, comboWidth, 300, hwnd, (HMENU)ID_COMBO_DAY, hInstance, NULL);
    CreateWindowExW(0, L"STATIC", L"天", WS_CHILD | WS_VISIBLE | SS_LEFT, timeX_Start + comboWidth + comboLabelGap, yPos + 4, labelWidth, 30, hwnd, NULL, hInstance, NULL);
    
    int hX = timeX_Start + timeUnitWidth + timeUnitGap;
    hComboHour = CreateWindowExW(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_VISIBLE, hX, yPos, comboWidth, 300, hwnd, (HMENU)ID_COMBO_HOUR, hInstance, NULL);
    CreateWindowExW(0, L"STATIC", L"时", WS_CHILD | WS_VISIBLE | SS_LEFT, hX + comboWidth + comboLabelGap, yPos + 4, labelWidth, 30, hwnd, NULL, hInstance, NULL);
    
    int mX = timeX_Start + 2 * (timeUnitWidth + timeUnitGap);
    hComboMinute = CreateWindowExW(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_CHILD | WS_VISIBLE, mX, yPos, comboWidth, 300, hwnd, (HMENU)ID_COMBO_MINUTE, hInstance, NULL);
    CreateWindowExW(0, L"STATIC", L"分", WS_CHILD | WS_VISIBLE | SS_LEFT, mX + comboWidth + comboLabelGap, yPos + 4, labelWidth, 30, hwnd, NULL, hInstance, NULL);
    
    PopulateComboBoxes();
    yPos += 50;

    hLabelCountdown = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, yPos, WINDOW_W - 40, 45, hwnd, NULL, hInstance, NULL);
    yPos += 55;

    int checkWidth = 120;
    hCheckNotify = CreateWindowExW(0, L"BUTTON", L"结束前提示", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, (WINDOW_W - checkWidth) / 2, yPos, checkWidth, 25, hwnd, (HMENU)ID_CHECK_NOTIFY, hInstance, NULL);
    yPos += 40;

    int btnWidth = 90, btnGap = 20;
    int btnX_Start = (WINDOW_W - (2 * btnWidth + btnGap)) / 2;
    hBtnStart = CreateWindowExW(0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, btnX_Start, yPos, btnWidth, 30, hwnd, (HMENU)ID_BUTTON_START, hInstance, NULL);
    hBtnClear = CreateWindowExW(0, L"BUTTON", L"清除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, btnX_Start + btnWidth + btnGap, yPos, btnWidth, 30, hwnd, (HMENU)ID_BUTTON_CLEAR, hInstance, NULL);

    EnumChildWindows(hwnd, SetChildFont, (LPARAM)g_hFontUI);
    SendMessageW(hLabelCountdown, WM_SETFONT, (WPARAM)g_hFontCountdown, TRUE);
}

void CreateTrayIcon(HWND hwnd, HICON hIcon)
{
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = ID_ICON;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_ICON_MSG;
    nid.hIcon = hIcon;
    lstrcpyW(nid.szTip, L"定时关机");
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
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

BOOL CALLBACK SetChildFont(HWND hwnd, LPARAM lParam)
{
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

void PopulateComboBoxes()
{
    wchar_t b[5];
    for (int i = 0; i < 100; ++i)
    {
        wsprintfW(b, L"%02d", i);
        SendMessageW(hComboDay, CB_ADDSTRING, 0, (LPARAM)b);
        if (i < 60) SendMessageW(hComboMinute, CB_ADDSTRING, 0, (LPARAM)b);
        if (i < 24) SendMessageW(hComboHour, CB_ADDSTRING, 0, (LPARAM)b);
    }
    SendMessageW(hComboDay, CB_SETCURSEL, 0, 0);
    SendMessageW(hComboHour, CB_SETCURSEL, 0, 0);
    SendMessageW(hComboMinute, CB_SETCURSEL, 0, 0);
}

void UpdateCountdownDisplay()
{
    if (g_TotalSeconds <= 0 && !g_isTimerRunning)
    {
        SetWindowTextW(hLabelCountdown, L"");
        return;
    }
    
    // 【优化】改为 int，避免 64 位除法
    int r = g_TotalSeconds;
    int d = r / 86400; r %= 86400;
    int h = r / 3600; r %= 3600;
    int m = r / 60;
    int s = r % 60;

    wchar_t ts[100] = {0}, t[20];
    if (d > 0) { wsprintfW(t, L"%d天 ", d); lstrcatW(ts, t); }
    if (h > 0 || d > 0) { wsprintfW(t, L"%02d时 ", h); lstrcatW(ts, t); }
    if (m > 0 || h > 0 || d > 0) { wsprintfW(t, L"%02d分 ", m); lstrcatW(ts, t); }
    wsprintfW(t, L"%02d秒", s); lstrcatW(ts, t);

    const wchar_t *os = (g_selectedOperation == OP_REBOOT) ? L"重启" : 
                        (g_selectedOperation == OP_SLEEP) ? L"睡眠" : L"关机";
    
    wchar_t b[200];
    wsprintfW(b, L"将在 %s 后 %s", ts, os);
    SetWindowTextW(hLabelCountdown, b);
}

void StartCountdown(HWND hwnd)
{
    // 【优化】移除 long long 强制转换，完全使用 int
    g_TotalSeconds = SendMessageW(hComboDay, CB_GETCURSEL, 0, 0) * 86400 + 
                     SendMessageW(hComboHour, CB_GETCURSEL, 0, 0) * 3600 + 
                     SendMessageW(hComboMinute, CB_GETCURSEL, 0, 0) * 60;
                     
    if (g_TotalSeconds <= 0)
    {
        MessageBoxW(hwnd, L"请设置一个有效的倒计时长。", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    g_isTimerRunning = TRUE;
    g_isPaused = FALSE;
    g_bNotificationShown = FALSE;
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
    g_bNotificationShown = FALSE;
    g_TotalSeconds = 0;
    SetWindowTextW(hBtnStart, L"确定");
    SendMessageW(hComboDay, CB_SETCURSEL, 0, 0);
    SendMessageW(hComboHour, CB_SETCURSEL, 0, 0);
    SendMessageW(hComboMinute, CB_SETCURSEL, 0, 0);
    SendMessageW(hRadioShutdown, BM_SETCHECK, BST_CHECKED, 0);
    g_selectedOperation = OP_SHUTDOWN;
    SendMessageW(hCheckNotify, BM_SETCHECK, BST_UNCHECKED, 0);
    g_bNotifyBeforeEnd = FALSE;
    SetControlsEnabled(TRUE);
    SetWindowTextW(hLabelCountdown, L"");
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
        ShellExecuteW(NULL, NULL, L"shutdown", L"/s /t 0 /f", NULL, SW_HIDE);
        break;
    case OP_REBOOT:
        ShellExecuteW(NULL, NULL, L"shutdown", L"/r /t 0 /f", NULL, SW_HIDE);
        break;
    case OP_SLEEP:
        ShellExecuteW(NULL, NULL, L"rundll32.exe", L"powrprof.dll,SetSuspendState 0,1,0", NULL, SW_HIDE);
        break;
    }
}