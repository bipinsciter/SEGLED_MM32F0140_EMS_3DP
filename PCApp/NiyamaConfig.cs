// NIYAMA_3DP configuration and monitoring tool.
//
// Single-file WinForms application, built against .NET Framework 4.x with the C#
// compiler that ships inside Windows, so the result is one .exe that runs on any
// Windows 8 or later machine with nothing installed.
//
// Protocol is the one described in UART_Protocol.docx, taken from the firmware:
//   request   FF  ID  CMD  PID  [payload]  CRC  FE
//   response  FD  ID  CMD  STATUS  PID  [payload]  CRC  FC
//   CRC       (0x55 + sum of bytes 1..n-2), masked to 0x7F when it exceeds it
//
// Build:  csc /target:winexe /out:NiyamaConfig.exe NiyamaConfig.cs

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace NiyamaConfig
{
    // ---------------------------------------------------------------- protocol

    static class Proto
    {
        public const byte REQ_START = 0xFF, REQ_END = 0xFE;
        public const byte RSP_START = 0xFD, RSP_END = 0xFC;
        public const byte CMD_READ = 0x10, CMD_WRITE = 0x11;

        public const byte ST_INVALID_PARA = 0x02, ST_DP1_FAULT = 0x04, ST_RH_TM_FAULT = 0x08;
        public const byte ST_DP2_FAULT = 0x10, ST_DP3_FAULT = 0x20, ST_RTC_INVALID = 0x40;

        public const byte ID_REALTIME = 0x48, ID_SFVER = 0x34;
        public const byte ID_HOUR = 0x11, ID_MIN = 0x12, ID_SEC = 0x13;
        public const byte ID_YEAR = 0x14, ID_MONTH = 0x15, ID_DATE = 0x16;

        // The real-time response is BINARY and fixed length, so it must be read by
        // length.  Scanning for the 0xFC terminator would truncate it the moment a
        // float happened to contain that byte.
        public const int REALTIME_LEN = 51;

        public static byte Crc(byte[] buf, int offset, int count)
        {
            uint total = 0x55;
            for (int i = 0; i < count; i++) total += buf[offset + i];
            if (total > 0x7F) total &= 0x7F;
            return (byte)total;
        }

        public static byte[] BuildRead(byte devId, byte pid, string payload)
        {
            return Build(devId, CMD_READ, pid, payload);
        }

        public static byte[] BuildWrite(byte devId, byte pid, string payload)
        {
            return Build(devId, CMD_WRITE, pid, payload);
        }

        static byte[] Build(byte devId, byte cmd, byte pid, string payload)
        {
            var body = new List<byte> { devId, cmd, pid };
            if (!string.IsNullOrEmpty(payload))
                body.AddRange(Encoding.ASCII.GetBytes(payload));

            var frame = new List<byte> { REQ_START };
            frame.AddRange(body);
            var arr = body.ToArray();
            frame.Add(Crc(arr, 0, arr.Length));
            frame.Add(REQ_END);
            return frame.ToArray();
        }

        public class Response
        {
            public byte DeviceId, Command, Status, ParamId;
            public byte[] Payload = new byte[0];
            public string Text = "";
            public bool CrcOk;
            public bool InvalidPara { get { return (Status & ST_INVALID_PARA) != 0; } }
        }

        /// Pull one response out of the buffer. expectedLen > 0 means a fixed-length
        /// binary reply; otherwise the frame ends at the first 0xFC whose CRC checks
        /// out, which tolerates a stray terminator inside the data.
        public static Response TryParse(List<byte> rx, int expectedLen)
        {
            int start = rx.IndexOf(RSP_START);
            if (start < 0) { rx.Clear(); return null; }
            if (start > 0) rx.RemoveRange(0, start);

            if (expectedLen > 0)
            {
                if (rx.Count < expectedLen) return null;
                var b = rx.GetRange(0, expectedLen).ToArray();
                rx.RemoveRange(0, expectedLen);
                return Decode(b);
            }

            for (int end = 2; end < rx.Count; end++)
            {
                if (rx[end] != RSP_END) continue;
                var b = rx.GetRange(0, end + 1).ToArray();
                if (Crc(b, 1, b.Length - 3) != b[b.Length - 2]) continue;  // stray 0xFC
                rx.RemoveRange(0, end + 1);
                return Decode(b);
            }
            return null;
        }

        static Response Decode(byte[] b)
        {
            var r = new Response();
            if (b.Length < 7) return r;
            r.DeviceId = b[1]; r.Command = b[2]; r.Status = b[3]; r.ParamId = b[4];
            r.CrcOk = Crc(b, 1, b.Length - 3) == b[b.Length - 2];
            int n = b.Length - 7;
            if (n > 0)
            {
                r.Payload = new byte[n];
                Array.Copy(b, 5, r.Payload, 0, n);
                r.Text = Encoding.ASCII.GetString(r.Payload).Trim();
            }
            return r;
        }

        public static string StatusText(byte s)
        {
            if (s == 0) return "OK";
            var parts = new List<string>();
            if ((s & ST_INVALID_PARA) != 0) parts.Add("INVALID PARAM");
            if ((s & ST_DP1_FAULT) != 0) parts.Add("DP1 fault");
            if ((s & ST_RH_TM_FAULT) != 0) parts.Add("Temp/RH fault");
            if ((s & ST_DP2_FAULT) != 0) parts.Add("DP2 fault");
            if ((s & ST_DP3_FAULT) != 0) parts.Add("DP3 fault");
            if ((s & ST_RTC_INVALID) != 0) parts.Add("RTC invalid");
            return string.Join(", ", parts);
        }
    }

    // ---------------------------------------------------------------- parameters

    enum Acc { R, W, RW }

    class Param
    {
        public byte Id;
        public string Name, Group, Units;
        public double Scale = 1;      // wire value / Scale = engineering value
        public Acc Access = Acc.RW;
        public int Decimals = 0;
        public string Note = "";

        public Param(string group, byte id, string name, string units, double scale,
                     int dec, Acc acc)
        {
            Group = group; Id = id; Name = name; Units = units;
            Scale = scale; Decimals = dec; Access = acc;
        }
    }

    static class Params
    {
        public static readonly List<Param> All = new List<Param>
        {
            // ---- alarm setpoints: stored in tenths, carried on the wire in hundredths
            new Param("Alarm - DP1", 0x02, "Upper alarm ON",  "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP1", 0x01, "Upper alarm OFF", "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP1", 0x04, "Lower alarm ON",  "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP1", 0x03, "Lower alarm OFF", "Pa", 100, 2, Acc.RW),

            new Param("Alarm - DP2", 0x06, "Upper alarm ON",  "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP2", 0x05, "Upper alarm OFF", "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP2", 0x08, "Lower alarm ON",  "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP2", 0x07, "Lower alarm OFF", "Pa", 100, 2, Acc.RW),

            new Param("Alarm - DP3", 0x62, "Upper alarm ON",  "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP3", 0x61, "Upper alarm OFF", "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP3", 0x64, "Lower alarm ON",  "Pa", 100, 2, Acc.RW),
            new Param("Alarm - DP3", 0x63, "Lower alarm OFF", "Pa", 100, 2, Acc.RW),

            new Param("Alarm - Temperature", 0x0A, "Upper alarm ON",  "deg", 100, 2, Acc.RW),
            new Param("Alarm - Temperature", 0x09, "Upper alarm OFF", "deg", 100, 2, Acc.RW),
            new Param("Alarm - Temperature", 0x0C, "Lower alarm ON",  "deg", 100, 2, Acc.RW),
            new Param("Alarm - Temperature", 0x0B, "Lower alarm OFF", "deg", 100, 2, Acc.RW),

            new Param("Alarm - Humidity", 0x0E, "Upper alarm ON",  "%RH", 100, 2, Acc.RW),
            new Param("Alarm - Humidity", 0x0D, "Upper alarm OFF", "%RH", 100, 2, Acc.RW),
            new Param("Alarm - Humidity", 0x10, "Lower alarm ON",  "%RH", 100, 2, Acc.RW),
            new Param("Alarm - Humidity", 0x0F, "Lower alarm OFF", "%RH", 100, 2, Acc.RW),

            // ---- device
            new Param("Device", 0x1A, "Device ID",            "",        1, 0, Acc.RW),
            new Param("Device", 0x19, "Log interval",         "min",     1, 0, Acc.RW),
            new Param("Device", 0x2E, "Temp unit (0=C 1=F)",  "",        1, 0, Acc.RW),
            new Param("Device", 0x41, "Baud code (0-9)",      "",        1, 0, Acc.RW),
            new Param("Device", 0x58, "Master enable",        "",        1, 0, Acc.RW),
            new Param("Device", 0x34, "Firmware version",     "",        1, 0, Acc.R ),

            // ---- buzzer / alarm behaviour
            new Param("Buzzer", 0x35, "Buzzer ON time",  "s", 1, 0, Acc.RW),
            new Param("Buzzer", 0x36, "Buzzer OFF time", "s", 1, 0, Acc.RW),
            new Param("Buzzer", 0x3A, "Ack silence time","s", 1, 0, Acc.RW),

            // ---- sensing times
            new Param("Sensing", 0x5C, "DP1 alarm sensing time", "s", 1, 0, Acc.RW),
            new Param("Sensing", 0x5D, "DP2 alarm sensing time", "s", 1, 0, Acc.RW),
            new Param("Sensing", 0x68, "DP3 alarm sensing time", "s", 1, 0, Acc.RW),
            new Param("Sensing", 0x5B, "Door sensing time",      "s", 1, 0, Acc.RW),
            new Param("Sensing", 0x5A, "Door polarity",          "",  1, 0, Acc.RW),

            // ---- display / comms
            new Param("Display", 0x5E, "LCD brightness (0-15)", "", 1, 0, Acc.RW),
            new Param("Display", 0x6F, "LCD off (1=off)",       "", 1, 0, Acc.RW),
            new Param("Comms",   0x70, "UART disable (1=off)",  "", 1, 0, Acc.RW),
            new Param("Comms",   0x69, "Auto-send interval",    "min", 1, 0, Acc.RW),
            new Param("Comms",   0x6A, "XBee reset interval",   "min", 1, 0, Acc.RW),
            new Param("Comms",   0x6C, "Devices in group",      "", 1, 0, Acc.RW),
        };
    }

    // ---------------------------------------------------------------- main form

    public class MainForm : Form
    {
        SerialPort port;
        readonly List<byte> rx = new List<byte>();

        ComboBox cboPort, cboBaud;
        NumericUpDown numDevId;
        Button btnConnect;
        Label lblLink, lblStatus;
        Timer pollTimer;

        Label lblFw, lblRtc, lblEpoch;
        Label[] lblVal = new Label[3], lblMin = new Label[3], lblMax = new Label[3],
                lblAlm = new Label[3], lblChan = new Label[3];

        DataGridView grid;
        TextBox txtLog;
        CheckBox chkPoll;
        ComboBox cboSlotCh, cboSlotIdx;
        NumericUpDown numSlotVal, numLimitVal;
        ComboBox cboLimitCh;

        static readonly string[] BaudNames =
            { "1200","2400","4800","9600","14400","19200","28800","38400","57600","115200" };
        static readonly int[] BaudRates =
            { 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200 };

        public MainForm()
        {
            Text = "NIYAMA_3DP Configuration Tool";
            Size = new Size(940, 680);
            MinimumSize = new Size(820, 560);
            Font = new Font("Segoe UI", 9f);
            StartPosition = FormStartPosition.CenterScreen;

            Controls.Add(BuildTabs());
            Controls.Add(BuildConnectionBar());
            Controls.Add(BuildStatusBar());

            pollTimer = new Timer { Interval = 1000 };
            pollTimer.Tick += (s, e) => PollLive();

            FormClosing += (s, e) => Disconnect();
            RefreshPorts();
        }

        // ------------------------------------------------ connection bar

        Control BuildConnectionBar()
        {
            var p = new Panel { Dock = DockStyle.Top, Height = 44, Padding = new Padding(8, 8, 8, 6) };

            cboPort = new Combo(70);
            var btnRescan = new Button { Text = "Rescan", Width = 62, Height = 24 };
            btnRescan.Click += (s, e) => RefreshPorts();

            cboBaud = new Combo(78);
            cboBaud.Items.AddRange(BaudNames);
            cboBaud.SelectedIndex = 8;                       // 57600, the firmware default

            numDevId = new NumericUpDown { Width = 52, Minimum = 0, Maximum = 250, Value = 1, Height = 24 };

            btnConnect = new Button { Text = "Connect", Width = 86, Height = 24 };
            btnConnect.Click += (s, e) => { if (port != null && port.IsOpen) Disconnect(); else Connect(); };

            lblLink = new Label { Text = "Disconnected", AutoSize = true, ForeColor = Color.Firebrick,
                                  Padding = new Padding(10, 5, 0, 0) };

            var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            flow.Controls.Add(Lbl("Port")); flow.Controls.Add(cboPort); flow.Controls.Add(btnRescan);
            flow.Controls.Add(Lbl("   Baud")); flow.Controls.Add(cboBaud);
            flow.Controls.Add(Lbl("   Device ID")); flow.Controls.Add(numDevId);
            flow.Controls.Add(btnConnect);
            flow.Controls.Add(lblLink);
            p.Controls.Add(flow);
            return p;
        }

        Control BuildStatusBar()
        {
            lblStatus = new Label { Dock = DockStyle.Bottom, Height = 22, Text = "Ready",
                                    BorderStyle = BorderStyle.Fixed3D, Padding = new Padding(6, 4, 0, 0) };
            return lblStatus;
        }

        static Label Lbl(string t)
        {
            return new Label { Text = t, AutoSize = true, Padding = new Padding(0, 5, 2, 0) };
        }

        static ComboBox Combo(int w)
        {
            return new ComboBox { Width = w, Height = 24, DropDownStyle = ComboBoxStyle.DropDownList };
        }

        // ------------------------------------------------ tabs

        Control BuildTabs()
        {
            var tabs = new TabControl { Dock = DockStyle.Fill };
            tabs.TabPages.Add(LiveTab());
            tabs.TabPages.Add(ParamTab());
            tabs.TabPages.Add(IndexedTab());
            tabs.TabPages.Add(LogTab());
            return tabs;
        }

        TabPage LiveTab()
        {
            var tp = new TabPage("Live") { Padding = new Padding(10) };

            var head = new TableLayoutPanel { Dock = DockStyle.Top, Height = 78, ColumnCount = 6 };
            head.Controls.Add(Cap("Firmware"), 0, 0);
            lblFw = Big("-"); head.Controls.Add(lblFw, 1, 0);
            head.Controls.Add(Cap("Device time"), 2, 0);
            lblRtc = Big("-"); head.Controls.Add(lblRtc, 3, 0);
            head.Controls.Add(Cap("Epoch"), 4, 0);
            lblEpoch = Big("-"); head.Controls.Add(lblEpoch, 5, 0);

            var g = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 5, RowCount = 4,
                                           CellBorderStyle = TableLayoutPanelCellBorderStyle.Single };
            string[] hdr = { "Channel", "Value", "Minimum", "Maximum", "Alarm" };
            for (int c = 0; c < hdr.Length; c++)
                g.Controls.Add(new Label { Text = hdr[c], Font = new Font(Font, FontStyle.Bold),
                                           AutoSize = true, Padding = new Padding(6, 6, 6, 6) }, c, 0);

            for (int i = 0; i < 3; i++)
            {
                lblChan[i] = Cell("-"); lblVal[i] = Cell("-"); lblMin[i] = Cell("-");
                lblMax[i] = Cell("-"); lblAlm[i] = Cell("-");
                lblVal[i].Font = new Font("Segoe UI", 14f, FontStyle.Bold);
                g.Controls.Add(lblChan[i], 0, i + 1);
                g.Controls.Add(lblVal[i], 1, i + 1);
                g.Controls.Add(lblMin[i], 2, i + 1);
                g.Controls.Add(lblMax[i], 3, i + 1);
                g.Controls.Add(lblAlm[i], 4, i + 1);
            }

            chkPoll = new CheckBox { Text = "Poll live values every second", Checked = true,
                                     Dock = DockStyle.Bottom, Height = 26, Padding = new Padding(4, 4, 0, 0) };
            chkPoll.CheckedChanged += (s, e) =>
            {
                if (chkPoll.Checked && port != null && port.IsOpen) pollTimer.Start(); else pollTimer.Stop();
            };

            tp.Controls.Add(g);
            tp.Controls.Add(chkPoll);
            tp.Controls.Add(head);
            return tp;
        }

        static Label Cap(string t)
        {
            return new Label { Text = t, AutoSize = true, ForeColor = Color.DimGray,
                               Padding = new Padding(8, 10, 4, 0) };
        }

        static Label Big(string t)
        {
            return new Label { Text = t, AutoSize = true, Font = new Font("Segoe UI", 12f, FontStyle.Bold),
                               Padding = new Padding(2, 6, 18, 0) };
        }

        static Label Cell(string t)
        {
            return new Label { Text = t, AutoSize = true, Padding = new Padding(8, 8, 8, 8) };
        }

        TabPage ParamTab()
        {
            var tp = new TabPage("Parameters") { Padding = new Padding(8) };

            grid = new DataGridView
            {
                Dock = DockStyle.Fill,
                AllowUserToAddRows = false,
                AllowUserToDeleteRows = false,
                RowHeadersVisible = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect,
                AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
                BackgroundColor = Color.White,
            };
            grid.Columns.Add(Col("grp", "Group", 90, true));
            grid.Columns.Add(Col("nam", "Parameter", 150, true));
            grid.Columns.Add(Col("pid", "ID", 40, true));
            grid.Columns.Add(Col("val", "Value", 80, false));
            grid.Columns.Add(Col("unt", "Units", 50, true));
            grid.Columns.Add(Col("acc", "Access", 50, true));

            foreach (var p in Params.All)
            {
                int r = grid.Rows.Add(p.Group, p.Name, "0x" + p.Id.ToString("X2"), "", p.Units, p.Access.ToString());
                grid.Rows[r].Tag = p;
                if (p.Access == Acc.R) grid.Rows[r].Cells["val"].ReadOnly = true;
            }

            var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 36 };
            var bRead = new Button { Text = "Read all", Width = 90 };
            var bReadSel = new Button { Text = "Read selected", Width = 110 };
            var bWriteSel = new Button { Text = "Write selected", Width = 110 };
            bRead.Click += (s, e) => ReadParams(grid.Rows.Cast<DataGridViewRow>());
            bReadSel.Click += (s, e) => ReadParams(grid.SelectedRows.Cast<DataGridViewRow>());
            bWriteSel.Click += (s, e) => WriteParams(grid.SelectedRows.Cast<DataGridViewRow>());
            bar.Controls.Add(bRead); bar.Controls.Add(bReadSel); bar.Controls.Add(bWriteSel);
            bar.Controls.Add(new Label { Text = "Edit the Value cell, then Write selected.",
                                         AutoSize = true, ForeColor = Color.DimGray,
                                         Padding = new Padding(12, 8, 0, 0) });

            tp.Controls.Add(grid);
            tp.Controls.Add(bar);
            return tp;
        }

        static DataGridViewTextBoxColumn Col(string name, string header, int w, bool ro)
        {
            return new DataGridViewTextBoxColumn { Name = name, HeaderText = header,
                                                   FillWeight = w, ReadOnly = ro };
        }

        TabPage IndexedTab()
        {
            var tp = new TabPage("Indexed") { Padding = new Padding(14) };
            var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown,
                                             WrapContents = false };

            flow.Controls.Add(new Label
            {
                AutoSize = true, MaximumSize = new Size(820, 0),
                Text = "These parameters carry index bytes between the parameter ID and the value, "
                     + "so one identifier addresses several stored values.",
                ForeColor = Color.DimGray, Padding = new Padding(0, 0, 0, 10)
            });

            // ---- DP limit (0x6E): one channel index, value in tenths of a Pa
            var g1 = new GroupBox { Text = "DP reading clamp  (0x6E, tenths of a Pa)", Width = 540, Height = 70 };
            cboLimitCh = Combo(70); cboLimitCh.Items.AddRange(new object[] { "DP1", "DP2", "DP3" });
            cboLimitCh.SelectedIndex = 0; cboLimitCh.Location = new Point(12, 28);
            numLimitVal = new NumericUpDown { Location = new Point(96, 28), Width = 90,
                                              Minimum = 0, Maximum = 9990, Value = 3000 };
            var bLimRd = new Button { Text = "Read", Location = new Point(200, 27), Width = 64 };
            var bLimWr = new Button { Text = "Write", Location = new Point(270, 27), Width = 64 };
            bLimRd.Click += (s, e) => ReadLimit();
            bLimWr.Click += (s, e) => WriteLimit();
            g1.Controls.AddRange(new Control[] { cboLimitCh, numLimitVal, bLimRd, bLimWr });
            g1.Controls.Add(new Label { Text = "3000 = 300.0 Pa", AutoSize = true,
                                        Location = new Point(344, 31), ForeColor = Color.DimGray });

            // ---- per-slot DP offset (0x72): channel + slot, value in tenths of a Pa
            var g2 = new GroupBox { Text = "Per-slot DP offset  (0x72, tenths of a Pa)", Width = 540, Height = 92 };
            cboSlotCh = Combo(70); cboSlotCh.Items.AddRange(new object[] { "DP1", "DP2", "DP3" });
            cboSlotCh.SelectedIndex = 0; cboSlotCh.Location = new Point(12, 28);
            cboSlotIdx = Combo(150);
            cboSlotIdx.Items.AddRange(new object[] {
                "0:  |DP| < 50 Pa", "1:  |DP| < 100 Pa", "2:  |DP| < 150 Pa",
                "3:  |DP| < 200 Pa", "4:  |DP| < 250 Pa", "5:  |DP| < 300 Pa" });
            cboSlotIdx.SelectedIndex = 0; cboSlotIdx.Location = new Point(96, 28);
            numSlotVal = new NumericUpDown { Location = new Point(258, 28), Width = 90,
                                             Minimum = -5000, Maximum = 5000, Value = 0 };
            var bSlotRd = new Button { Text = "Read", Location = new Point(360, 27), Width = 64 };
            var bSlotWr = new Button { Text = "Write", Location = new Point(430, 27), Width = 64 };
            bSlotRd.Click += (s, e) => ReadSlot();
            bSlotWr.Click += (s, e) => WriteSlot();
            g2.Controls.AddRange(new Control[] { cboSlotCh, cboSlotIdx, numSlotVal, bSlotRd, bSlotWr });
            g2.Controls.Add(new Label
            {
                Text = "Offset added to readings whose magnitude falls in the chosen slot. 15 = +1.5 Pa.",
                AutoSize = true, Location = new Point(12, 60), ForeColor = Color.DimGray
            });

            flow.Controls.Add(g1);
            flow.Controls.Add(new Label { Height = 8, Width = 10 });
            flow.Controls.Add(g2);
            tp.Controls.Add(flow);
            return tp;
        }

        TabPage LogTab()
        {
            var tp = new TabPage("Log") { Padding = new Padding(8) };
            txtLog = new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
                                   ScrollBars = ScrollBars.Vertical, Font = new Font("Consolas", 8.5f),
                                   BackColor = Color.White };
            var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 34 };
            var bClear = new Button { Text = "Clear", Width = 80 };
            bClear.Click += (s, e) => txtLog.Clear();
            bar.Controls.Add(bClear);
            tp.Controls.Add(txtLog);
            tp.Controls.Add(bar);
            return tp;
        }

        // ------------------------------------------------ serial

        void RefreshPorts()
        {
            string keep = cboPort.SelectedItem as string;
            cboPort.Items.Clear();
            cboPort.Items.AddRange(SerialPort.GetPortNames().OrderBy(n => n).ToArray());
            if (keep != null && cboPort.Items.Contains(keep)) cboPort.SelectedItem = keep;
            else if (cboPort.Items.Count > 0) cboPort.SelectedIndex = 0;
        }

        void Connect()
        {
            if (cboPort.SelectedItem == null) { Say("No serial port selected."); return; }
            try
            {
                port = new SerialPort((string)cboPort.SelectedItem,
                                      BaudRates[cboBaud.SelectedIndex], Parity.None, 8, StopBits.One);
                port.ReadTimeout = 400;
                port.WriteTimeout = 400;
                port.Open();
                btnConnect.Text = "Disconnect";
                lblLink.Text = "Connected to " + port.PortName + " @ " + port.BaudRate;
                lblLink.ForeColor = Color.ForestGreen;
                Say("Connected.");
                ReadIdentity();
                if (chkPoll.Checked) pollTimer.Start();
            }
            catch (Exception ex) { Say("Open failed: " + ex.Message); port = null; }
        }

        void Disconnect()
        {
            pollTimer.Stop();
            try { if (port != null && port.IsOpen) port.Close(); } catch { }
            port = null;
            btnConnect.Text = "Connect";
            lblLink.Text = "Disconnected";
            lblLink.ForeColor = Color.Firebrick;
        }

        bool Ready { get { return port != null && port.IsOpen; } }

        /// One request, one response. Everything goes through here so the log and the
        /// status line stay honest about what actually happened on the wire.
        Proto.Response Exchange(byte[] frame, int expectedLen)
        {
            if (!Ready) { Say("Not connected."); return null; }
            try
            {
                rx.Clear();
                port.DiscardInBuffer();
                port.Write(frame, 0, frame.Length);
                Log("TX  " + Hex(frame));

                var deadline = DateTime.UtcNow.AddMilliseconds(600);
                while (DateTime.UtcNow < deadline)
                {
                    int avail = port.BytesToRead;
                    if (avail > 0)
                    {
                        var buf = new byte[avail];
                        int got = port.Read(buf, 0, avail);
                        for (int i = 0; i < got; i++) rx.Add(buf[i]);

                        var r = Proto.TryParse(rx, expectedLen);
                        if (r != null)
                        {
                            Log("RX  status=" + Proto.StatusText(r.Status) + (r.CrcOk ? "" : "  [CRC FAIL]"));
                            if (!r.CrcOk) Say("Response failed its checksum.");
                            else if (r.InvalidPara) Say("Device rejected the parameter.");
                            return r;
                        }
                    }
                    else System.Threading.Thread.Sleep(5);
                }
                Log("RX  (timeout)");
                Say("No response from device.");
                return null;
            }
            catch (Exception ex) { Say("Serial error: " + ex.Message); return null; }
        }

        static string Hex(byte[] b)
        {
            return string.Join(" ", b.Select(x => x.ToString("X2")));
        }

        void Log(string s)
        {
            if (txtLog.TextLength > 200000) txtLog.Clear();
            txtLog.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + s + Environment.NewLine);
        }

        void Say(string s) { lblStatus.Text = s; }

        // ------------------------------------------------ operations

        void ReadIdentity()
        {
            var r = Exchange(Proto.BuildRead((byte)numDevId.Value, Proto.ID_SFVER, null), 0);
            if (r != null && r.CrcOk)
            {
                int v;
                if (int.TryParse(r.Text, out v))
                    lblFw.Text = string.Format("{0}.{1}.{2}", v / 100, (v / 10) % 10, v % 10);
                else lblFw.Text = r.Text;
            }
        }

        void PollLive()
        {
            if (!Ready) return;

            var r = Exchange(Proto.BuildRead((byte)numDevId.Value, Proto.ID_REALTIME, null),
                             Proto.REALTIME_LEN);
            if (r == null || !r.CrcOk || r.Payload.Length < 44) return;

            var p = r.Payload;                                  // payload starts at frame[5]
            uint epoch = BitConverter.ToUInt32(p, 0);
            var utc = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).AddSeconds(epoch);
            lblEpoch.Text = epoch.ToString();
            lblRtc.Text = utc.ToString("yyyy-MM-dd HH:mm:ss");

            bool tempMode = (r.Status & Proto.ST_RH_TM_FAULT) != 0 || TempRhLayout;
            string[] names = tempMode
                ? new[] { "DP1 (Pa)", "Temperature", "Humidity (%RH)" }
                : new[] { "DP1 (Pa)", "DP2 (Pa)", "DP3 (Pa)" };

            float[] now = { BitConverter.ToSingle(p, 5), BitConverter.ToSingle(p, 9),
                            BitConverter.ToSingle(p, 13) };
            float[] mins = { BitConverter.ToSingle(p, 17), BitConverter.ToSingle(p, 25),
                             BitConverter.ToSingle(p, 33) };
            float[] maxs = { BitConverter.ToSingle(p, 21), BitConverter.ToSingle(p, 29),
                             BitConverter.ToSingle(p, 37) };
            byte[] alm = { p[41], p[42], p[43] };

            for (int i = 0; i < 3; i++)
            {
                lblChan[i].Text = names[i];
                lblVal[i].Text = now[i].ToString("0.0", CultureInfo.InvariantCulture);
                lblMin[i].Text = mins[i].ToString("0.0", CultureInfo.InvariantCulture);
                lblMax[i].Text = maxs[i].ToString("0.0", CultureInfo.InvariantCulture);
                lblAlm[i].Text = alm[i] == 1 ? "HIGH" : alm[i] == 2 ? "LOW" : "-";
                lblAlm[i].ForeColor = alm[i] == 0 ? Color.Black : Color.Firebrick;
                lblVal[i].ForeColor = alm[i] == 0 ? Color.Black : Color.Firebrick;
            }
            Say("Live: " + Proto.StatusText(r.Status));
        }

        /// The firmware lays the middle two channels out differently in the two build
        /// modes and the frame carries no flag saying which, so it is a user choice.
        bool TempRhLayout { get { return chkTempRh != null && chkTempRh.Checked; } }
        CheckBox chkTempRh;

        void ReadParams(IEnumerable<DataGridViewRow> rows)
        {
            foreach (var row in rows.ToList())
            {
                var p = row.Tag as Param;
                if (p == null || p.Access == Acc.W) continue;
                var r = Exchange(Proto.BuildRead((byte)numDevId.Value, p.Id, null), 0);
                if (r == null || !r.CrcOk) { row.Cells["val"].Value = "?"; continue; }
                if (r.InvalidPara) { row.Cells["val"].Value = "n/a"; continue; }

                double v;
                if (double.TryParse(r.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out v))
                    row.Cells["val"].Value = (v / p.Scale).ToString("F" + p.Decimals, CultureInfo.InvariantCulture);
                else row.Cells["val"].Value = r.Text;
                Application.DoEvents();
            }
            Say("Read complete.");
        }

        void WriteParams(IEnumerable<DataGridViewRow> rows)
        {
            foreach (var row in rows.ToList())
            {
                var p = row.Tag as Param;
                if (p == null || p.Access == Acc.R) continue;
                var cell = row.Cells["val"].Value;
                if (cell == null) continue;

                double v;
                if (!double.TryParse(cell.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out v))
                { Say("'" + cell + "' is not a number."); return; }

                long wire = (long)Math.Round(v * p.Scale);
                var r = Exchange(Proto.BuildWrite((byte)numDevId.Value, p.Id,
                                                  wire.ToString(CultureInfo.InvariantCulture)), 0);
                if (r == null || !r.CrcOk) { Say("Write failed for " + p.Name); return; }
                if (r.InvalidPara) { Say("Device rejected " + p.Name + " (out of range?)"); return; }
                Application.DoEvents();
            }
            Say("Write complete.");
        }

        // ---- indexed parameters

        void ReadLimit()
        {
            var r = Exchange(Proto.BuildRead((byte)numDevId.Value, 0x6E,
                                             cboLimitCh.SelectedIndex.ToString()), 0);
            int v;
            if (r != null && r.CrcOk && !r.InvalidPara && int.TryParse(r.Text, out v))
            {
                numLimitVal.Value = Math.Max(numLimitVal.Minimum, Math.Min(numLimitVal.Maximum, v));
                Say("DP limit = " + (v / 10.0).ToString("0.0") + " Pa");
            }
        }

        void WriteLimit()
        {
            string payload = cboLimitCh.SelectedIndex.ToString()
                           + ((int)numLimitVal.Value).ToString("D5");
            var r = Exchange(Proto.BuildWrite((byte)numDevId.Value, 0x6E, payload), 0);
            if (r != null && r.CrcOk && !r.InvalidPara) Say("DP limit written.");
        }

        void ReadSlot()
        {
            string payload = cboSlotCh.SelectedIndex.ToString() + cboSlotIdx.SelectedIndex.ToString();
            var r = Exchange(Proto.BuildRead((byte)numDevId.Value, 0x72, payload), 0);
            int v;
            if (r != null && r.CrcOk && !r.InvalidPara && int.TryParse(r.Text, out v))
            {
                numSlotVal.Value = Math.Max(numSlotVal.Minimum, Math.Min(numSlotVal.Maximum, v));
                Say("Slot offset = " + (v / 10.0).ToString("0.0") + " Pa");
            }
        }

        void WriteSlot()
        {
            int v = (int)numSlotVal.Value;
            // five characters for the value, with the sign inside the field
            string num = v < 0 ? "-" + Math.Abs(v).ToString("D4") : v.ToString("D5");
            string payload = cboSlotCh.SelectedIndex.ToString()
                           + cboSlotIdx.SelectedIndex.ToString() + num;
            var r = Exchange(Proto.BuildWrite((byte)numDevId.Value, 0x72, payload), 0);
            if (r != null && r.CrcOk && !r.InvalidPara) Say("Slot offset written.");
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}
