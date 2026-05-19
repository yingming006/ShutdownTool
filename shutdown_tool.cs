using System;
using System.Drawing;
using System.Windows.Forms;
using System.Diagnostics;
using System.Runtime.InteropServices;

class ShutdownTool : Form
{
    enum Operation { Shutdown, Reboot, Sleep }

    RadioButton rbShutdown, rbReboot, rbSleep;
    ComboBox cmbDay, cmbHour, cmbMinute;
    Label lblCountdown;
    CheckBox chkNotify;
    Button btnStart, btnClear, btnMinimize, btnExit;
    NotifyIcon trayIcon;
    System.Windows.Forms.Timer timer;
    ContextMenuStrip trayMenu;
    Icon trayIconFile;

    int totalSeconds = 0;
    bool isTimerRunning = false;
    bool isPaused = false;
    bool notificationShown = false;
    Operation selectedOperation = Operation.Shutdown;

    const int NOTIFICATION_SECONDS = 55;
    const int SW_RESTORE = 9;

    const int WIN_W = 430, WIN_H = 280;
    const int MARGIN = 20;

    const int RADIO_Y = 20, RADIO_W = 70, RADIO_H = 30;
    const int RADIO_SD_X = 100, RADIO_RB_X = 180, RADIO_SP_X = 260;

    const int COMBO_Y = 70, COMBO_W = 65, COMBO_H = 22;
    const int COMBO_D_X = 75, COMBO_H_X = 185, COMBO_M_X = 295;
    const int LBL_D_X = 142, LBL_H_X = 252, LBL_M_X = 362;

    const int CDOWN_Y = 130, CDOWN_H = 45;

    const int NOTIFY_X = 155, NOTIFY_Y = 185;

    const int BTN_Y = 225, BTN_W = 90, BTN_H = 30;
    const int BTN_START_X = 20, BTN_CLEAR_X = 120, BTN_MIN_X = 220, BTN_EXIT_X = 320;

    [DllImport("user32.dll")]
    static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll")]
    static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    static extern bool SetForegroundWindow(IntPtr hWnd);

    [STAThread]
    static void Main()
    {
        IntPtr hwnd = FindWindow(null, "定时关机");
        if (hwnd != IntPtr.Zero)
        {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            return;
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new ShutdownTool());
    }

    public ShutdownTool()
    {
        trayIconFile = new Icon("icon.ico");
        Icon = trayIconFile;
        InitUI();
    }

    void InitUI()
    {
        Text = "定时关机";
        ClientSize = new Size(WIN_W, WIN_H);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(240, 240, 240);

        var uiFont = new Font("Segoe UI", 12f, FontStyle.Regular);
        var countFont = new Font("Segoe UI", 20f, FontStyle.Bold);
        Font = uiFont;

        rbShutdown = new RadioButton { Text = "关机", Location = new Point(RADIO_SD_X, RADIO_Y), Size = new Size(RADIO_W, RADIO_H), Checked = true, BackColor = BackColor };
        rbReboot = new RadioButton { Text = "重启", Location = new Point(RADIO_RB_X, RADIO_Y), Size = new Size(RADIO_W, RADIO_H), BackColor = BackColor };
        rbSleep = new RadioButton { Text = "睡眠", Location = new Point(RADIO_SP_X, RADIO_Y), Size = new Size(RADIO_W, RADIO_H), BackColor = BackColor };
        rbShutdown.CheckedChanged += OnRadioChanged;
        rbReboot.CheckedChanged += OnRadioChanged;
        rbSleep.CheckedChanged += OnRadioChanged;

        cmbDay = MakeCombo(COMBO_D_X, COMBO_Y, COMBO_W);
        cmbHour = MakeCombo(COMBO_H_X, COMBO_Y, COMBO_W);
        cmbMinute = MakeCombo(COMBO_M_X, COMBO_Y, COMBO_W);

        var lblDay = new Label { Text = "天", Location = new Point(LBL_D_X, COMBO_Y + 4), AutoSize = true, BackColor = BackColor };
        var lblHour = new Label { Text = "时", Location = new Point(LBL_H_X, COMBO_Y + 4), AutoSize = true, BackColor = BackColor };
        var lblMin = new Label { Text = "分", Location = new Point(LBL_M_X, COMBO_Y + 4), AutoSize = true, BackColor = BackColor };

        for (int i = 0; i < 100; i++)
        {
            string s = i.ToString("D2");
            cmbDay.Items.Add(s);
            if (i < 24) cmbHour.Items.Add(s);
            if (i < 60) cmbMinute.Items.Add(s);
        }
        cmbDay.SelectedIndex = 0;
        cmbHour.SelectedIndex = 0;
        cmbMinute.SelectedIndex = 0;

        lblCountdown = new Label
        {
            Location = new Point(MARGIN, CDOWN_Y),
            Size = new Size(WIN_W - MARGIN * 2, CDOWN_H),
            TextAlign = ContentAlignment.MiddleCenter,
            Font = countFont,
            BackColor = BackColor
        };

        chkNotify = new CheckBox
        {
            Text = "结束前提示",
            Location = new Point(NOTIFY_X, NOTIFY_Y),
            Size = new Size(120, 25),
            BackColor = BackColor
        };

        btnStart = new Button { Text = "确定", Location = new Point(BTN_START_X, BTN_Y), Size = new Size(BTN_W, BTN_H) };
        btnStart.Click += OnStartClick;

        btnClear = new Button { Text = "清除", Location = new Point(BTN_CLEAR_X, BTN_Y), Size = new Size(BTN_W, BTN_H) };
        btnClear.Click += OnClearClick;

        btnExit = new Button { Text = "退出", Location = new Point(BTN_EXIT_X, BTN_Y), Size = new Size(BTN_W, BTN_H) };
        btnExit.Click += OnExitClick;

        btnMinimize = new Button { Text = "最小化", Location = new Point(BTN_MIN_X, BTN_Y), Size = new Size(BTN_W, BTN_H) };
        btnMinimize.Click += (s, e) => HideWindow();

        Controls.AddRange(new Control[] {
            rbShutdown, rbReboot, rbSleep,
            cmbDay, cmbHour, cmbMinute,
            lblDay, lblHour, lblMin,
            lblCountdown, chkNotify,
            btnStart, btnClear, btnExit, btnMinimize
        });

        timer = new System.Windows.Forms.Timer { Interval = 1000 };
        timer.Tick += OnTimerTick;

        trayMenu = new ContextMenuStrip();
        trayMenu.Items.Add("关于", null, (s, e) => Process.Start("https://yingming006.github.io/ShutdownTool/"));
        trayMenu.Items.Add("退出", null, (s, e) => { trayIcon.Visible = false; Application.Exit(); });

        trayIcon = new NotifyIcon { Text = "定时关机", Icon = trayIconFile, Visible = false, ContextMenuStrip = trayMenu };
        trayIcon.MouseClick += (s, e) =>
        {
            if (e.Button == MouseButtons.Left)
            {
                ShowWindow();
            }
        };

        FormClosing += (s, e) =>
        {
            if (e.CloseReason == CloseReason.UserClosing)
            {
                e.Cancel = true;
                HideWindow();
            }
        };

        Resize += (s, e) =>
        {
            if (WindowState == FormWindowState.Minimized)
                HideWindow();
        };
    }

    ComboBox MakeCombo(int x, int y, int w)
    {
        return new ComboBox
        {
            Location = new Point(x, y),
            Size = new Size(w, COMBO_H),
            DropDownStyle = ComboBoxStyle.DropDownList,
            MaxDropDownItems = 8,
            IntegralHeight = false,
            Font = Font
        };
    }

    void HideWindow()
    {
        Hide();
        trayIcon.Visible = true;
    }

    void ShowWindow()
    {
        trayIcon.Visible = false;
        Show();
        WindowState = FormWindowState.Normal;
        Activate();
    }

    void OnRadioChanged(object sender, EventArgs e)
    {
        if (rbShutdown.Checked) selectedOperation = Operation.Shutdown;
        else if (rbReboot.Checked) selectedOperation = Operation.Reboot;
        else if (rbSleep.Checked) selectedOperation = Operation.Sleep;
    }

    void OnStartClick(object sender, EventArgs e)
    {
        if (!isTimerRunning)
        {
            int d = cmbDay.SelectedIndex;
            int h = cmbHour.SelectedIndex;
            int m = cmbMinute.SelectedIndex;
            totalSeconds = d * 86400 + h * 3600 + m * 60;

            if (totalSeconds <= 0)
            {
                MessageBox.Show("请设置一个有效的倒计时长。", "提示",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            isTimerRunning = true;
            isPaused = false;
            notificationShown = false;
            timer.Start();
            btnStart.Text = "暂停";
            SetControlsEnabled(false);
            UpdateCountdown();
        }
        else
        {
            isPaused = !isPaused;
            btnStart.Text = isPaused ? "继续" : "暂停";
        }
    }

    void OnClearClick(object sender, EventArgs e)
    {
        timer.Stop();
        isTimerRunning = false;
        isPaused = false;
        notificationShown = false;
        totalSeconds = 0;
        btnStart.Text = "确定";
        cmbDay.SelectedIndex = 0;
        cmbHour.SelectedIndex = 0;
        cmbMinute.SelectedIndex = 0;
        rbShutdown.Checked = true;
        chkNotify.Checked = false;
        SetControlsEnabled(true);
        lblCountdown.Text = "";
    }

    void OnExitClick(object sender, EventArgs e)
    {
        timer.Stop();
        try { RunSilent("shutdown", "/a"); } catch { }
        trayIcon.Visible = false;
        Application.Exit();
    }

    void SetControlsEnabled(bool enable)
    {
        rbShutdown.Enabled = enable;
        rbReboot.Enabled = enable;
        rbSleep.Enabled = enable;
        cmbDay.Enabled = enable;
        cmbHour.Enabled = enable;
        cmbMinute.Enabled = enable;
    }

    void OnTimerTick(object sender, EventArgs e)
    {
        if (isPaused) return;

        if (chkNotify.Checked && !notificationShown && totalSeconds == NOTIFICATION_SECONDS)
        {
            notificationShown = true;
            string op = selectedOperation == Operation.Reboot ? "重启"
                      : selectedOperation == Operation.Sleep ? "睡眠" : "关机";
            MessageBox.Show(
                string.Format("程序将在 {0} 秒后执行【{1}】操作。\n请及时保存您的工作！",
                    NOTIFICATION_SECONDS, op),
                "操作提醒",
                MessageBoxButtons.OK,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button1,
                MessageBoxOptions.DefaultDesktopOnly);
        }

        if (--totalSeconds < 0) totalSeconds = 0;
        UpdateCountdown();

        if (totalSeconds == 0)
        {
            timer.Stop();
            DoAction();
            Application.Exit();
        }
    }

    void UpdateCountdown()
    {
        if (totalSeconds <= 0 && !isTimerRunning)
        {
            lblCountdown.Text = "";
            return;
        }

        int r = totalSeconds;
        int d = r / 86400; r %= 86400;
        int h = r / 3600; r %= 3600;
        int m = r / 60;
        int s = r % 60;

        string ts = "";
        if (d > 0) ts += d + "天 ";
        if (h > 0 || d > 0) ts += h.ToString("D2") + "时 ";
        if (m > 0 || h > 0 || d > 0) ts += m.ToString("D2") + "分 ";
        ts += s.ToString("D2") + "秒";

        string op = selectedOperation == Operation.Reboot ? "重启"
                  : selectedOperation == Operation.Sleep ? "睡眠" : "关机";

        lblCountdown.Text = string.Format("将在 {0} 后 {1}", ts, op);
    }

    void DoAction()
    {
        try
        {
            switch (selectedOperation)
            {
                case Operation.Shutdown:
                    RunSilent("shutdown", "/s /t 0 /f");
                    break;
                case Operation.Reboot:
                    RunSilent("shutdown", "/r /t 0 /f");
                    break;
                case Operation.Sleep:
                    RunSilent("rundll32.exe", "powrprof.dll,SetSuspendState 0,1,0");
                    break;
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show("执行失败：" + ex.Message, "错误",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    void RunSilent(string file, string args)
    {
        var psi = new ProcessStartInfo(file, args)
        {
            CreateNoWindow = true,
            WindowStyle = ProcessWindowStyle.Hidden
        };
        Process.Start(psi);
    }

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ClassStyle |= 0x200;
            return cp;
        }
    }
}