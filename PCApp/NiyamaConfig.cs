// NIYAMA_3DP configuration and monitoring tool.
//
// One WinForms source file, built against .NET Framework 4.x with the C# compiler
// that ships inside Windows, so the result is a single .exe that runs on any
// Windows 8 or later machine with nothing installed.
//
// Protocol, taken from ServePCMsg() in main.c:
//   request   FF  ID  CMD  PID  [payload]  CRC  FE
//   response  FD  ID  CMD  STATUS  PID  [payload]  CRC  FC
//   CRC       0x55 + sum of bytes 1..n-2, masked to 0x7F when it exceeds it
//   CMD       0x10 read, 0x11 write
//
// Scaling is not uniform, so every entry in the parameter table carries its own
// factor:
//   alarm setpoints  wire = hundredths of the unit (firmware stores tenths)
//   min / max        wire = hundredths
//   DP limit, slot offset   wire = tenths;   zero offset   wire = hundredths
//   everything else  wire = the raw number
//
// Build:  csc /target:winexe /out:NiyamaConfig.exe NiyamaConfig.cs

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
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

        public const byte ID_SFVER = 0x34, ID_DATETIME = 0x4B, ID_REALTIME = 0x48;
        public const byte ID_TMUNIT = 0x2E, ID_DVCID = 0x1A, ID_BAUD = 0x41;
        public const byte ID_DP_LIMIT = 0x6E, ID_DP_SLOT_OFFSET = 0x72;
        public const byte ID_DP_OFFSET = 0x71, ID_CAL_CPWD = 0x38, ID_CAL_FPWD = 0x37;
        public const byte ID_SRNO = 0x40, ID_TMCAL = 0x32, ID_RHCAL = 0x33;
        public const byte ID_DFLT_CAL = 0x52;

        //The serial number reply is a fixed 23 bytes: header, 16 ASCII characters,
        //checksum, terminator.  Those 16 bytes are free-form, so it is read by length.
        public const int SRNO_LEN = 23;
        public const int SRNO_CHARS = 16;

        //A calibration reply carries the stored correction and, when calibration is
        //unlocked, the calibration date history behind it - so its length depends on
        //which mode is open.
        public const int CAL_LEN_LOCKED = 12;
        public const int CAL_LEN_FACTORY = 39;
        public const int CAL_LEN_CUSTOMER = 72;

        // The real-time reply is BINARY and fixed length, so it has to be read by
        // length.  Scanning for the 0xFC terminator would truncate it the moment a
        // float happened to contain that byte.
        public const int REALTIME_LEN = 51;
        public const int DATETIME_LEN = 19;

        public static byte Crc(byte[] buf, int offset, int count)
        {
            uint total = 0x55;
            for (int i = 0; i < count; i++) total += buf[offset + i];
            if (total > 0x7F) total &= 0x7F;
            return (byte)total;
        }

        public static byte[] BuildRead(byte devId, byte pid, string payload)
        { return Build(devId, CMD_READ, pid, payload); }

        public static byte[] BuildWrite(byte devId, byte pid, string payload)
        { return Build(devId, CMD_WRITE, pid, payload); }

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
            public byte[] Raw = new byte[0];
            public bool InvalidPara { get { return (Status & ST_INVALID_PARA) != 0; } }
        }

        /// Pull one response out of the buffer.  expectedLen > 0 means a fixed-length
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

            for (int end = 6; end < rx.Count; end++)
            {
                if (rx[end] != RSP_END) continue;
                var b = rx.GetRange(0, end + 1).ToArray();
                if (Crc(b, 1, b.Length - 3) != b[b.Length - 2]) continue;   // stray 0xFC
                rx.RemoveRange(0, end + 1);
                return Decode(b);
            }
            return null;
        }

        static Response Decode(byte[] b)
        {
            var r = new Response { Raw = b };
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

        /// The firmware parses every ASCII value with findValue(), which returns an
        /// int16_t.  Anything outside that range wraps silently on the device, so a
        /// value that does not fit must never be sent.
        public static bool FitsOnWire(long v) { return v >= -32768 && v <= 32767; }

        /// A five character value field with the sign, if any, inside it.  This is what
        /// the firmware's findValue(ptr, 5) expects: it consumes a leading '-' out of the
        /// five, leaving four digits, so a negative number carries one digit fewer.
        public static string Field5(long v)
        {
            return v < 0 ? "-" + Math.Abs(v).ToString("D4", CultureInfo.InvariantCulture)
                         : v.ToString("D5", CultureInfo.InvariantCulture);
        }

        /// A sign byte of its own followed by five digits, used by the zero offset, where
        /// the firmware reads the sign from the byte ahead of the value field.
        public static string SignedField5(long v)
        {
            return (v < 0 ? "-" : "+") + Math.Abs(v).ToString("D5", CultureInfo.InvariantCulture);
        }

        public static string StatusText(byte s)
        {
            if (s == 0) return "OK";
            var parts = new List<string>();
            if ((s & ST_INVALID_PARA) != 0) parts.Add("invalid parameter");
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
    enum Mode { Unknown, TempRh, ThreeDp }

    class Param
    {
        public byte Id;
        public string Name, Group, Units, Hint;
        public double Scale;
        public int Decimals;
        public Acc Access;
        public Mode Only;          // Mode.Unknown = present in both firmware builds

        public Param(string group, byte id, string name, string units, double scale,
                     int dec, Acc acc, Mode only, string hint)
        {
            Group = group; Id = id; Name = name; Units = units; Scale = scale;
            Decimals = dec; Access = acc; Only = only; Hint = hint;
        }

        public Param(string group, byte id, string name, string units, double scale,
                     int dec, Acc acc)
            : this(group, id, name, units, scale, dec, acc, Mode.Unknown, "") { }

        public Param(string group, byte id, string name, string units, double scale,
                     int dec, Acc acc, Mode only)
            : this(group, id, name, units, scale, dec, acc, only, "") { }
    }

    static class Params
    {
        public static readonly List<Param> All = new List<Param>
        {
            // ---- alarm setpoints.  Firmware stores tenths; the wire carries hundredths.
            new Param("Alarm DP1", 0x02, "Upper alarm ON",  "Pa", 100, 1, Acc.RW),
            new Param("Alarm DP1", 0x01, "Upper alarm OFF", "Pa", 100, 1, Acc.RW),
            new Param("Alarm DP1", 0x03, "Lower alarm OFF", "Pa", 100, 1, Acc.RW),
            new Param("Alarm DP1", 0x04, "Lower alarm ON",  "Pa", 100, 1, Acc.RW),

            new Param("Alarm DP2", 0x06, "Upper alarm ON",  "Pa", 100, 1, Acc.RW, Mode.ThreeDp),
            new Param("Alarm DP2", 0x05, "Upper alarm OFF", "Pa", 100, 1, Acc.RW, Mode.ThreeDp),
            new Param("Alarm DP2", 0x07, "Lower alarm OFF", "Pa", 100, 1, Acc.RW, Mode.ThreeDp),
            new Param("Alarm DP2", 0x08, "Lower alarm ON",  "Pa", 100, 1, Acc.RW, Mode.ThreeDp),

            new Param("Alarm DP3", 0x62, "Upper alarm ON",  "Pa", 100, 1, Acc.RW, Mode.ThreeDp),
            new Param("Alarm DP3", 0x61, "Upper alarm OFF", "Pa", 100, 1, Acc.RW, Mode.ThreeDp),
            new Param("Alarm DP3", 0x63, "Lower alarm OFF", "Pa", 100, 1, Acc.RW, Mode.ThreeDp),
            new Param("Alarm DP3", 0x64, "Lower alarm ON",  "Pa", 100, 1, Acc.RW, Mode.ThreeDp),

            new Param("Alarm Temp", 0x0A, "Upper alarm ON",  "deg", 100, 1, Acc.RW, Mode.TempRh),
            new Param("Alarm Temp", 0x09, "Upper alarm OFF", "deg", 100, 1, Acc.RW, Mode.TempRh),
            new Param("Alarm Temp", 0x0B, "Lower alarm OFF", "deg", 100, 1, Acc.RW, Mode.TempRh),
            new Param("Alarm Temp", 0x0C, "Lower alarm ON",  "deg", 100, 1, Acc.RW, Mode.TempRh),

            new Param("Alarm RH", 0x0E, "Upper alarm ON",  "%RH", 100, 1, Acc.RW, Mode.TempRh),
            new Param("Alarm RH", 0x0D, "Upper alarm OFF", "%RH", 100, 1, Acc.RW, Mode.TempRh),
            new Param("Alarm RH", 0x0F, "Lower alarm OFF", "%RH", 100, 1, Acc.RW, Mode.TempRh),
            new Param("Alarm RH", 0x10, "Lower alarm ON",  "%RH", 100, 1, Acc.RW, Mode.TempRh),

            // ---- recorded extremes (read only)
            new Param("Min / Max", 0x1C, "DP1 minimum", "Pa", 100, 2, Acc.R),
            new Param("Min / Max", 0x1D, "DP1 maximum", "Pa", 100, 2, Acc.R),
            new Param("Min / Max", 0x1E, "DP2 minimum", "Pa", 100, 2, Acc.R, Mode.ThreeDp),
            new Param("Min / Max", 0x1F, "DP2 maximum", "Pa", 100, 2, Acc.R, Mode.ThreeDp),
            new Param("Min / Max", 0x65, "DP3 minimum", "Pa", 100, 2, Acc.R, Mode.ThreeDp),
            new Param("Min / Max", 0x66, "DP3 maximum", "Pa", 100, 2, Acc.R, Mode.ThreeDp),
            new Param("Min / Max", 0x20, "Temp minimum", "deg", 100, 2, Acc.R, Mode.TempRh),
            new Param("Min / Max", 0x21, "Temp maximum", "deg", 100, 2, Acc.R, Mode.TempRh),
            new Param("Min / Max", 0x22, "RH minimum", "%RH", 100, 2, Acc.R, Mode.TempRh),
            new Param("Min / Max", 0x23, "RH maximum", "%RH", 100, 2, Acc.R, Mode.TempRh),

            // ---- device
            new Param("Device", 0x34, "Firmware version", "", 1, 0, Acc.R),
            new Param("Device", 0x1A, "Device ID", "", 1, 0, Acc.RW, Mode.Unknown,
                      "Sent twice - the firmware ignores a single write."),
            new Param("Device", 0x41, "Baud rate code 0-9", "", 1, 0, Acc.RW, Mode.Unknown,
                      "Takes effect at once - reconnect at the new rate."),
            new Param("Device", 0x2E, "Temperature unit 0=C 1=F", "", 1, 0, Acc.RW, Mode.TempRh),
            new Param("Device", 0x19, "Log interval", "min", 1, 0, Acc.RW),
            new Param("Device", 0x58, "Master enable", "", 1, 0, Acc.RW),
            new Param("Device", 0x54, "24 h mean start hour", "h", 1, 0, Acc.RW),

            // ---- alarm behaviour
            new Param("Buzzer", 0x35, "Buzzer ON time", "s", 1, 0, Acc.RW, Mode.Unknown,
                      "Zero in either time silences the sounder."),
            new Param("Buzzer", 0x36, "Buzzer OFF time", "s", 1, 0, Acc.RW),
            new Param("Buzzer", 0x3A, "Acknowledge silence time", "s", 1, 0, Acc.RW),

            // ---- sensing
            new Param("Sensing", 0x5C, "DP1 alarm sensing time", "s", 1, 0, Acc.RW),
            new Param("Sensing", 0x5D, "DP2 alarm sensing time", "s", 1, 0, Acc.RW, Mode.ThreeDp),
            new Param("Sensing", 0x68, "DP3 alarm sensing time", "s", 1, 0, Acc.RW, Mode.ThreeDp),
            new Param("Sensing", 0x5B, "Door sensing time", "s", 1, 0, Acc.RW),
            new Param("Sensing", 0x5A, "Door contact polarity", "", 1, 0, Acc.RW),

            // ---- display and comms
            new Param("Display", 0x5E, "LCD brightness 0-15", "", 1, 0, Acc.RW),
            new Param("Display", 0x6F, "LCD off when 1", "", 1, 0, Acc.RW),
            new Param("Display", 0x4F, "Scroll time", "s", 1, 0, Acc.RW),
            new Param("Comms", 0x70, "UART disabled when 1", "", 1, 0, Acc.RW),
            new Param("Comms", 0x69, "Auto-send interval", "min", 1, 0, Acc.RW),
            new Param("Comms", 0x6A, "Radio reset interval", "min", 1, 0, Acc.RW),
            new Param("Comms", 0x6C, "Devices in group", "", 1, 0, Acc.RW),
        };
    }

    // ---------------------------------------------------------------- link

    /// Owns the serial port and the request pacing.
    ///
    /// The device services its UART from a main loop that takes roughly 80 ms a pass.
    /// Requests arriving faster than that overrun its receive buffer and it stops
    /// answering until the line falls quiet again.  Measured on hardware: a steady
    /// 80 ms gap ran 60 frames without a single loss, while faster rates - and rates
    /// that beat against the loop period - lost frames in long runs.  A miss is cleared
    /// by leaving the line alone; a short rest clears most, and a longer one clears the
    /// rest, so the retries back off 300 ms then 1200 ms.
    class Link
    {
        public const int InterFrameMs = 85;
        //Back-off before each retry.  A short pause clears most misses; when it does
        //not, the device has gone deaf for longer and only a real rest brings it back.
        static readonly int[] BackoffMs = { 300, 1200 };
        public const int Attempts = 3;
        public const int ReplyTimeoutMs = 500;

        readonly SerialPort port;
        readonly Action<string> log;
        readonly Func<bool> hexLog;
        readonly List<byte> rx = new List<byte>();
        DateTime lastExchange = DateTime.MinValue;

        public int Sent, Retried, Lost;

        public Link(SerialPort port, Action<string> log, Func<bool> hexLog)
        {
            this.port = port;
            this.log = log ?? delegate { };
            this.hexLog = hexLog ?? delegate { return false; };
        }

        public bool Ready { get { return port != null && port.IsOpen; } }

        /// One request, one response - paced so the device keeps up, retried when a
        /// reply goes missing.
        public Proto.Response Exchange(byte[] frame, int expectedLen, string what)
        {
            if (!Ready) return null;
            Sent++;

            for (int attempt = 1; attempt <= Attempts; attempt++)
            {
                // Kept in double: before the first exchange this span is centuries, which
                // overflows an int and turns the gap negative.
                double since = (DateTime.UtcNow - lastExchange).TotalMilliseconds;
                if (since < InterFrameMs)
                    System.Threading.Thread.Sleep((int)(InterFrameMs - since));

                var r = Once(frame, expectedLen, what);
                lastExchange = DateTime.UtcNow;

                if (r != null && r.CrcOk)
                {
                    if (attempt > 1) Retried++;
                    return r;
                }

                if (attempt < Attempts)
                {
                    int rest = BackoffMs[Math.Min(attempt - 1, BackoffMs.Length - 1)];
                    log(what + ": no usable reply (attempt " + attempt + " of " + Attempts
                        + ") - letting the line rest " + rest + " ms.");
                    System.Threading.Thread.Sleep(rest);
                    try { port.DiscardInBuffer(); } catch { }
                    lastExchange = DateTime.UtcNow;
                }
                else
                {
                    Lost++;
                    log(what + ": gave up after " + Attempts + " attempts.");
                    if (r != null) return r;     // answered, but the checksum never held
                }
            }
            return null;
        }

        Proto.Response Once(byte[] frame, int expectedLen, string what)
        {
            try
            {
                rx.Clear();
                port.DiscardInBuffer();
                port.Write(frame, 0, frame.Length);
                if (hexLog()) log("TX  " + Hex(frame));

                //A bulk reply is over a kilobyte, which at 57600 baud is a quarter of a
                //second of transmission on its own. Give long replies the time they
                //physically need on the wire, on top of the device's own turnaround.
                int budget = ReplyTimeoutMs + (expectedLen > 0 ? (expectedLen * 10000) / 57600 : 0);
                var deadline = DateTime.UtcNow.AddMilliseconds(budget);
                while (DateTime.UtcNow < deadline)
                {
                    if (port.BytesToRead > 0)
                    {
                        var buf = new byte[port.BytesToRead];
                        int got = port.Read(buf, 0, buf.Length);
                        for (int i = 0; i < got; i++) rx.Add(buf[i]);

                        var r = Proto.TryParse(rx, expectedLen);
                        if (r != null)
                        {
                            if (hexLog()) log("RX  " + Hex(r.Raw));

                            //Both replies echo the parameter id at byte 4.  If it is not the
                            //one just asked for, this is a late reply to an earlier request
                            //and pairing it with this one would show a value under the wrong
                            //name.  Drop it and let the retry re-ask.
                            if (r.CrcOk && frame.Length > 3 && r.ParamId != frame[3])
                            {
                                log(what + ": reply carries parameter 0x" + r.ParamId.ToString("X2")
                                    + " but 0x" + frame[3].ToString("X2") + " was requested - discarded.");
                                return null;
                            }

                            if (!r.CrcOk) log(what + ": the reply failed its checksum.");
                            else if (r.InvalidPara)
                                log(what + ": the device reported an invalid parameter.");
                            return r;
                        }
                    }
                    else System.Threading.Thread.Sleep(3);
                }
                return null;
            }
            catch (Exception ex) { log(what + ": serial error - " + ex.Message); return null; }
        }

        public static string Hex(byte[] b)
        { return string.Join(" ", b.Select(x => x.ToString("X2"))); }
    }

    // ---------------------------------------------------------------- main form

    public class MainForm : Form
    {
        SerialPort port;
        Link link;
        Mode deviceMode = Mode.Unknown;
        bool busy;                      // keeps the poll tick out of a manual exchange

        ComboBox cboPort, cboBaud;
        NumericUpDown numDevId;
        Button btnConnect;
        Label lblLink, lblStatus;
        Timer pollTimer;

        Label lblFw, lblRtc, lblMode;
        Label[] lblVal = new Label[3], lblMin = new Label[3], lblMax = new Label[3],
                lblAlm = new Label[3], lblChan = new Label[3];

        DataGridView grid, slotGrid;
        TextBox txtLogOut;
        NumericUpDown numRingCount, numDayCount;
        CheckBox chkLogRegular, chkLogRing, chkLogRam, chkLogDays, chkLogMeans;
        DateTimePicker dtFrom, dtTo;
        ProgressBar barLogs;
        TextBox txtLog;
        CheckBox chkPoll, chkHexLog;
        ComboBox cboSlotCh, cboLimitCh, cboOffCh;
        NumericUpDown numLimitVal, numOffVal, numCalPwd;
        TextBox txtSrNo;
        NumericUpDown numTmRef, numRhRef;
        Label lblTmCal, lblRhCal;
        CheckBox chkFactory;

        //What the application believes it has unlocked, and when.  The device closes
        //the window about 60 seconds after the last calibration exchange, and the
        //reply length for a calibration read depends on which mode is open.
        DateTime calUnlockedAt = DateTime.MinValue;
        bool calFactory;

        static readonly string[] BaudNames =
            { "9600  (code 3)", "14400  (4)", "19200  (5)", "28800  (6)",
              "38400  (7)", "57600  (8)", "115200  (9)" };
        static readonly int[] BaudRates = { 9600, 14400, 19200, 28800, 38400, 57600, 115200 };

        static readonly string[] SlotNames = {
            "0:   below 50 Pa", "1:   50 - 100 Pa", "2:   100 - 150 Pa",
            "3:   150 - 200 Pa", "4:   200 - 250 Pa", "5:   250 - 300 Pa" };

        public MainForm()
        {
            Text = "NIYAMA_3DP Configuration Tool";
            Size = new Size(980, 710);
            MinimumSize = new Size(880, 600);
            Font = new Font("Segoe UI", 9f);
            StartPosition = FormStartPosition.CenterScreen;

            Controls.Add(BuildTabs());
            Controls.Add(BuildConnectionBar());
            Controls.Add(BuildStatusBar());

            pollTimer = new Timer { Interval = 1000 };
            pollTimer.Tick += delegate { PollLive(); };

            FormClosing += delegate { Disconnect(); };
            RefreshPorts();
            ApplyMode();
        }

        // ------------------------------------------------ chrome

        Control BuildConnectionBar()
        {
            var p = new Panel { Dock = DockStyle.Top, Height = 42, Padding = new Padding(8, 7, 8, 5) };

            cboPort = Combo(82);
            var btnRescan = new Button { Text = "Rescan", Width = 62, Height = 24 };
            btnRescan.Click += delegate { RefreshPorts(); };

            cboBaud = Combo(112);
            cboBaud.Items.AddRange(BaudNames);
            cboBaud.SelectedIndex = 5;                     // 57600, the firmware default

            numDevId = new NumericUpDown { Width = 52, Minimum = 0, Maximum = 250, Value = 1 };

            btnConnect = new Button { Text = "Connect", Width = 86, Height = 24 };
            btnConnect.Click += delegate { if (Ready) Disconnect(); else Connect(); };

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
        { return new Label { Text = t, AutoSize = true, Padding = new Padding(0, 5, 2, 0) }; }

        static ComboBox Combo(int w)
        { return new ComboBox { Width = w, DropDownStyle = ComboBoxStyle.DropDownList }; }

        Control BuildTabs()
        {
            var tabs = new TabControl { Dock = DockStyle.Fill };
            tabs.TabPages.Add(LiveTab());
            tabs.TabPages.Add(ParamTab());
            tabs.TabPages.Add(CalTab());
            tabs.TabPages.Add(LogsTab());
            tabs.TabPages.Add(LogTab());
            return tabs;
        }

        // ------------------------------------------------ live tab

        TabPage LiveTab()
        {
            var tp = new TabPage("Live") { Padding = new Padding(12) };

            var head = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, WrapContents = false };
            head.Controls.Add(Cap("Firmware"));    lblFw = Big("-");   head.Controls.Add(lblFw);
            head.Controls.Add(Cap("Device clock")); lblRtc = Big("-"); head.Controls.Add(lblRtc);
            head.Controls.Add(Cap("Channels"));    lblMode = Big("-"); head.Controls.Add(lblMode);

            var bar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 34, WrapContents = false };
            var bSync = new Button { Text = "Set device clock to PC time", Width = 178 };
            bSync.Click += delegate { SyncClock(); };
            var bRdClock = new Button { Text = "Read clock", Width = 88 };
            bRdClock.Click += delegate { ReadClock(); };
            chkPoll = new CheckBox { Text = "Poll every second", Checked = true,
                                     Padding = new Padding(10, 4, 0, 0), Width = 142 };
            chkPoll.CheckedChanged += delegate
            { if (chkPoll.Checked && Ready) pollTimer.Start(); else pollTimer.Stop(); };
            bar.Controls.Add(bSync); bar.Controls.Add(bRdClock); bar.Controls.Add(chkPoll);

            var bar2 = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 32, WrapContents = false };
            txtSrNo = new TextBox { Width = 190, MaxLength = Proto.SRNO_CHARS,
                                    Font = new Font("Consolas", 9.5f) };
            var bSrRd = new Button { Text = "Read", Width = 62 };
            var bSrWr = new Button { Text = "Write", Width = 62 };
            bSrRd.Click += delegate { ReadSerial(); };
            bSrWr.Click += delegate { WriteSerial(); };
            bar2.Controls.Add(new Label { Text = "Serial number", AutoSize = true,
                                          Padding = new Padding(2, 6, 6, 0) });
            bar2.Controls.Add(txtSrNo);
            bar2.Controls.Add(bSrRd);
            bar2.Controls.Add(bSrWr);
            bar2.Controls.Add(new Label { Text = "exactly " + Proto.SRNO_CHARS + " characters",
                                          AutoSize = true, ForeColor = Color.DimGray,
                                          Padding = new Padding(8, 7, 0, 0) });

            var g = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 5, RowCount = 4,
                                           CellBorderStyle = TableLayoutPanelCellBorderStyle.Single };
            for (int c = 0; c < 5; c++)
                g.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, c == 0 ? 28f : 18f));
            g.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
            for (int r = 0; r < 3; r++) g.RowStyles.Add(new RowStyle(SizeType.Percent, 33f));

            string[] hdr = { "Channel", "Value", "Minimum", "Maximum", "Alarm" };
            for (int c = 0; c < hdr.Length; c++)
                g.Controls.Add(new Label { Text = hdr[c], Font = new Font(Font, FontStyle.Bold),
                                           AutoSize = true, Padding = new Padding(6, 7, 6, 6) }, c, 0);

            for (int i = 0; i < 3; i++)
            {
                lblChan[i] = Cell("-"); lblVal[i] = Cell("-"); lblMin[i] = Cell("-");
                lblMax[i] = Cell("-"); lblAlm[i] = Cell("-");
                lblChan[i].Font = new Font("Segoe UI", 11f);
                lblVal[i].Font = new Font("Segoe UI", 18f, FontStyle.Bold);
                g.Controls.Add(lblChan[i], 0, i + 1);
                g.Controls.Add(lblVal[i], 1, i + 1);
                g.Controls.Add(lblMin[i], 2, i + 1);
                g.Controls.Add(lblMax[i], 3, i + 1);
                g.Controls.Add(lblAlm[i], 4, i + 1);
            }

            tp.Controls.Add(g);
            tp.Controls.Add(bar2);
            tp.Controls.Add(bar);
            tp.Controls.Add(head);
            return tp;
        }

        static Label Cap(string t)
        { return new Label { Text = t, AutoSize = true, ForeColor = Color.DimGray,
                             Padding = new Padding(10, 12, 2, 0) }; }

        static Label Big(string t)
        { return new Label { Text = t, AutoSize = true, Font = new Font("Segoe UI", 12f, FontStyle.Bold),
                             Padding = new Padding(2, 8, 10, 0) }; }

        static Label Cell(string t)
        { return new Label { Text = t, AutoSize = true, Padding = new Padding(8, 8, 8, 4) }; }

        // ------------------------------------------------ parameters tab

        TabPage ParamTab()
        {
            var tp = new TabPage("Parameters") { Padding = new Padding(8) };

            grid = new DataGridView
            {
                Dock = DockStyle.Fill,
                AllowUserToAddRows = false,
                AllowUserToDeleteRows = false,
                AllowUserToResizeRows = false,
                RowHeadersVisible = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect,
                AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
                BackgroundColor = Color.White,
                MultiSelect = true,
            };
            grid.Columns.Add(Col("grp", "Group", 80, true));
            grid.Columns.Add(Col("nam", "Parameter", 175, true));
            grid.Columns.Add(Col("pid", "ID", 32, true));
            grid.Columns.Add(Col("val", "Value", 65, false));
            grid.Columns.Add(Col("unt", "Units", 42, true));
            grid.Columns.Add(Col("acc", "Access", 44, true));
            grid.Columns.Add(Col("hnt", "Notes", 190, true));

            foreach (var p in Params.All)
            {
                int r = grid.Rows.Add(p.Group, p.Name, "0x" + p.Id.ToString("X2"), "",
                                      p.Units, p.Access.ToString(), p.Hint);
                grid.Rows[r].Tag = p;
                if (p.Access == Acc.R)
                {
                    grid.Rows[r].Cells["val"].ReadOnly = true;
                    grid.Rows[r].DefaultCellStyle.BackColor = Color.FromArgb(246, 246, 246);
                }
            }

            var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 36 };
            var bRead = new Button { Text = "Read all", Width = 82 };
            var bReadSel = new Button { Text = "Read selected", Width = 104 };
            var bWriteSel = new Button { Text = "Write selected", Width = 104 };
            bRead.Click += delegate { ReadParams(VisibleRows()); };
            bReadSel.Click += delegate { ReadParams(SelectedRows()); };
            bWriteSel.Click += delegate { WriteParams(SelectedRows()); };
            bar.Controls.Add(bRead); bar.Controls.Add(bReadSel); bar.Controls.Add(bWriteSel);
            bar.Controls.Add(new Label { Text = "Edit a Value cell, then Write selected.",
                                         AutoSize = true, ForeColor = Color.DimGray,
                                         Padding = new Padding(12, 9, 0, 0) });

            tp.Controls.Add(grid);
            tp.Controls.Add(bar);
            return tp;
        }

        List<DataGridViewRow> VisibleRows()
        { return grid.Rows.Cast<DataGridViewRow>().Where(r => r.Visible).ToList(); }

        List<DataGridViewRow> SelectedRows()
        {
            return grid.SelectedRows.Cast<DataGridViewRow>()
                       .OrderBy(r => r.Index).ToList();
        }

        static DataGridViewTextBoxColumn Col(string name, string header, int w, bool ro)
        {
            return new DataGridViewTextBoxColumn { Name = name, HeaderText = header,
                                                   FillWeight = w, ReadOnly = ro,
                                                   SortMode = DataGridViewColumnSortMode.NotSortable };
        }

        // ------------------------------------------------ calibration tab

        TabPage CalTab()
        {
            var tp = new TabPage("Calibration") { Padding = new Padding(12), AutoScroll = true };

            // ---- DP reading clamp
            var g1 = new GroupBox { Text = "DP reading clamp   (0x6E)", Left = 6, Top = 8,
                                    Width = 580, Height = 76 };
            cboLimitCh = Combo(66); cboLimitCh.Items.AddRange(new object[] { "DP1", "DP2", "DP3" });
            cboLimitCh.SelectedIndex = 0; cboLimitCh.Location = new Point(14, 30);
            //In Pa. The wire carries tenths, and the firmware rejects anything below
            //50.0 Pa or above 999.0 Pa.
            numLimitVal = new NumericUpDown { Location = new Point(92, 30), Width = 82,
                                              DecimalPlaces = 1, Increment = 0.1m,
                                              Minimum = 50.0m, Maximum = 999.0m,
                                              Value = 300.0m };
            var bLimRd = new Button { Text = "Read", Location = new Point(186, 29), Width = 62 };
            var bLimWr = new Button { Text = "Write", Location = new Point(254, 29), Width = 62 };
            bLimRd.Click += delegate { ReadLimit(); };
            bLimWr.Click += delegate { WriteLimit(); };
            g1.Controls.AddRange(new Control[] { cboLimitCh, numLimitVal, bLimRd, bLimWr,
                new Label { Text = "Pa, to 0.1   (50.0 to 999.0)", AutoSize = true,
                            Location = new Point(330, 34), ForeColor = Color.DimGray } });

            // ---- per-slot offsets
            var g2 = new GroupBox { Text = "Per-slot DP offset   (0x72)", Left = 6, Top = 94,
                                    Width = 580, Height = 252 };
            cboSlotCh = Combo(66); cboSlotCh.Items.AddRange(new object[] { "DP1", "DP2", "DP3" });
            cboSlotCh.SelectedIndex = 0; cboSlotCh.Location = new Point(14, 26);
            var bSlotRd = new Button { Text = "Read all slots", Location = new Point(92, 25), Width = 100 };
            var bSlotWr = new Button { Text = "Write all slots", Location = new Point(198, 25), Width = 100 };
            bSlotRd.Click += delegate { ReadSlots(); };
            bSlotWr.Click += delegate { WriteSlots(); };

            slotGrid = new DataGridView
            {
                Location = new Point(14, 56), Size = new Size(548, 182),
                AllowUserToAddRows = false, AllowUserToDeleteRows = false,
                AllowUserToResizeRows = false, RowHeadersVisible = false,
                BackgroundColor = Color.White,
                AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
            };
            slotGrid.Columns.Add(Col("slot", "Reading band", 150, true));
            slotGrid.Columns.Add(Col("off", "Offset (Pa)", 120, false));
            foreach (var n in SlotNames) slotGrid.Rows.Add(n, "0.0");

            //Typed in Pa and stored in tenths, so only one decimal carries. Tidy the
            //cell on the way out, and mark anything that is not a number rather than
            //quietly sending a zero for it.
            slotGrid.CellEndEdit += delegate(object s, DataGridViewCellEventArgs e)
            {
                if (e.ColumnIndex != 1) return;
                var cell = slotGrid.Rows[e.RowIndex].Cells[1];
                decimal pa;
                if (cell.Value != null
                    && decimal.TryParse(cell.Value.ToString().Trim(), NumberStyles.Float,
                                        CultureInfo.InvariantCulture, out pa))
                {
                    cell.Value = pa.ToString("0.0", CultureInfo.InvariantCulture);
                    cell.Style.ForeColor = Color.Black;
                }
                else cell.Style.ForeColor = Color.Firebrick;
            };
            g2.Controls.AddRange(new Control[] { cboSlotCh, bSlotRd, bSlotWr, slotGrid });

            // ---- customer calibration: zero offset
            var g3 = new GroupBox { Text = "Zero offset   (0x71) - needs the calibration password",
                                    Left = 6, Top = 356, Width = 580, Height = 110 };
            numCalPwd = new NumericUpDown { Location = new Point(196, 26), Width = 74,
                                            Minimum = 0, Maximum = 9999, Value = 100 };
            var bUnlock = new Button { Text = "Unlock (60 s)", Location = new Point(278, 25), Width = 94 };
            bUnlock.Click += delegate { Unlock(); };
            chkFactory = new CheckBox { Text = "factory", Location = new Point(378, 28),
                                        Width = 70, Checked = false };
            cboOffCh = Combo(66); cboOffCh.Items.AddRange(new object[] { "DP1", "DP2", "DP3" });
            cboOffCh.SelectedIndex = 0; cboOffCh.Location = new Point(14, 66);
            //Entered in Pa.  The wire carries hundredths, and that field is an int16
            //on the device, so +-327.67 Pa is the whole of the representable range.
            numOffVal = new NumericUpDown { Location = new Point(92, 66), Width = 82,
                                            DecimalPlaces = 2, Increment = 0.01m,
                                            Minimum = -327.67m, Maximum = 327.67m, Value = 0 };
            var bOffRd = new Button { Text = "Read", Location = new Point(186, 65), Width = 62 };
            var bOffWr = new Button { Text = "Write", Location = new Point(254, 65), Width = 62 };
            bOffRd.Click += delegate { ReadOffset(); };
            bOffWr.Click += delegate { WriteOffset(); };
            g3.Controls.AddRange(new Control[] {
                new Label { Text = "Customer calibration password", AutoSize = true,
                            Location = new Point(14, 30) },
                numCalPwd, bUnlock, chkFactory, cboOffCh, numOffVal, bOffRd, bOffWr,
                new Label { Text = "Pa, to 0.01", AutoSize = true,
                            Location = new Point(330, 70), ForeColor = Color.DimGray } });

            // ---- temperature and humidity calibration
            var g4 = new GroupBox { Text = "Temperature and humidity calibration   (0x32 / 0x33)",
                                    Left = 6, Top = 472, Width = 580, Height = 152 };
            g4.Controls.Add(new Label
            {
                AutoSize = true, MaximumSize = new Size(548, 0), Location = new Point(14, 22),
                ForeColor = Color.DimGray,
                Text = "Enter the TRUE value from your reference instrument, not a correction. "
                     + "The device works out the correction itself, as its own reading minus "
                     + "what you enter. Unlock calibration first."
            });

            lblTmCal = new Label { Location = new Point(196, 74), AutoSize = true, Text = "-" };
            numTmRef = new NumericUpDown { Location = new Point(300, 71), Width = 80,
                                           DecimalPlaces = 1, Increment = 0.1m,
                                           Minimum = -3276.8m, Maximum = 3276.7m, Value = 25.0m };
            var bTmRd = new Button { Text = "Read", Location = new Point(390, 70), Width = 62 };
            var bTmWr = new Button { Text = "Calibrate", Location = new Point(458, 70), Width = 76 };
            bTmRd.Click += delegate { ReadCal(Proto.ID_TMCAL, lblTmCal, "Temperature"); };
            bTmWr.Click += delegate { WriteCal(Proto.ID_TMCAL, numTmRef, lblTmCal, "Temperature"); };

            lblRhCal = new Label { Location = new Point(196, 110), AutoSize = true, Text = "-" };
            numRhRef = new NumericUpDown { Location = new Point(300, 107), Width = 80,
                                           DecimalPlaces = 1, Increment = 0.1m,
                                           Minimum = 0m, Maximum = 100m, Value = 50.0m };
            var bRhRd = new Button { Text = "Read", Location = new Point(390, 106), Width = 62 };
            var bRhWr = new Button { Text = "Calibrate", Location = new Point(458, 106), Width = 76 };
            bRhRd.Click += delegate { ReadCal(Proto.ID_RHCAL, lblRhCal, "Humidity"); };
            bRhWr.Click += delegate { WriteCal(Proto.ID_RHCAL, numRhRef, lblRhCal, "Humidity"); };

            g4.Controls.AddRange(new Control[] {
                new Label { Text = "Temperature", Location = new Point(14, 74), AutoSize = true },
                new Label { Text = "stored correction", Location = new Point(100, 74),
                            AutoSize = true, ForeColor = Color.DimGray },
                lblTmCal, numTmRef, bTmRd, bTmWr,
                new Label { Text = "Humidity", Location = new Point(14, 110), AutoSize = true },
                new Label { Text = "stored correction", Location = new Point(100, 110),
                            AutoSize = true, ForeColor = Color.DimGray },
                lblRhCal, numRhRef, bRhRd, bRhWr });

            tp.Controls.AddRange(new Control[] { g1, g2, g3, g4 });
            return tp;
        }

        // ------------------------------------------------ logs tab

        TabPage LogsTab()
        {
            var tp = new TabPage("Logs") { Padding = new Padding(10) };

            var top = new Panel { Dock = DockStyle.Top, Height = 132 };

            chkLogRegular = new CheckBox { Text = "Regular log, by date", Checked = true,
                                          Location = new Point(12, 10), Width = 150 };
            dtFrom = new DateTimePicker { Location = new Point(168, 8), Width = 150,
                                          Format = DateTimePickerFormat.Custom,
                                          CustomFormat = "dd-MMM-yyyy HH:mm",
                                          Value = DateTime.Now.Date };
            dtTo = new DateTimePicker { Location = new Point(326, 8), Width = 150,
                                        Format = DateTimePickerFormat.Custom,
                                        CustomFormat = "dd-MMM-yyyy HH:mm",
                                        Value = DateTime.Now.AddMinutes(5) };

            chkLogRing = new CheckBox { Text = "24 hour ring, newest", Checked = true,
                                        Location = new Point(12, 38), Width = 150 };
            numRingCount = new NumericUpDown { Location = new Point(168, 36), Width = 70,
                                               Minimum = 1, Maximum = 1440, Value = 60 };

            chkLogRam = new CheckBox { Text = "RAM buffer, last 30 readings", Checked = true,
                                       Location = new Point(12, 66), Width = 220 };

            chkLogDays = new CheckBox { Text = "15 day min/max/mean, days", Checked = true,
                                        Location = new Point(12, 94), Width = 170 };
            numDayCount = new NumericUpDown { Location = new Point(190, 92), Width = 50,
                                              Minimum = 1, Maximum = 15, Value = 15 };

            chkLogMeans = new CheckBox { Text = "24 hourly means", Checked = true,
                                         Location = new Point(326, 94), Width = 150 };

            var bRead = new Button { Text = "Read and build report", Location = new Point(500, 34),
                                     Width = 160, Height = 30 };
            bRead.Click += delegate { ReadLogsAndReport(); };

            barLogs = new ProgressBar { Location = new Point(500, 72), Width = 160, Height = 14 };

            top.Controls.AddRange(new Control[] {
                chkLogRegular, dtFrom, dtTo, chkLogRing, numRingCount, chkLogRam,
                chkLogDays, numDayCount, chkLogMeans, bRead, barLogs,
                new Label { Text = "from", Location = new Point(168, 30), AutoSize = true,
                            ForeColor = Color.DimGray },
                new Label { Text = "to", Location = new Point(326, 30), AutoSize = true,
                            ForeColor = Color.DimGray },
                new Label { Text = "records", Location = new Point(244, 40), AutoSize = true,
                            ForeColor = Color.DimGray } });

            txtLogOut = new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
                                      ScrollBars = ScrollBars.Both, WordWrap = false,
                                      Font = new Font("Consolas", 8.5f), BackColor = Color.White };

            var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 34 };
            var bSave = new Button { Text = "Save report and CSV", Width = 150 };
            bSave.Click += delegate { SaveReport(); };
            bar.Controls.Add(bSave);
            bar.Controls.Add(new Label {
                Text = "A log is only as good as the clock behind it, so the report leads with both.",
                AutoSize = true, ForeColor = Color.DimGray, Padding = new Padding(12, 8, 0, 0) });

            tp.Controls.Add(txtLogOut);
            tp.Controls.Add(bar);
            tp.Controls.Add(top);
            return tp;
        }

        ReportData lastReport;

        void ReadLogsAndReport()
        {
            if (!Ready) { Say("Not connected."); return; }

            Serialised(delegate
            {
                var d = new ReportData();
                Action<string> note = delegate(string s) { Log("logs: " + s); };

                barLogs.Value = 0;
                barLogs.Maximum = 100;
                txtLogOut.Text = "Reading...";
                Application.DoEvents();

                // ---- identity and clock first: the logs mean little without them
                d.DeviceId = DevId;
                var v = Exchange(Proto.BuildRead(DevId, Proto.ID_SFVER, null), 0, "version");
                int iv;
                if (Good(v) && int.TryParse(v.Text, out iv))
                    d.FirmwareVersion = string.Format("{0}.{1}.{2}", iv / 100, (iv / 10) % 10, iv % 10);

                var sr = Exchange(Proto.BuildRead(DevId, Proto.ID_SRNO, null),
                                  Proto.SRNO_LEN, "serial");
                if (sr != null && sr.CrcOk && sr.Payload.Length >= Proto.SRNO_CHARS)
                    d.SerialNumber = Encoding.ASCII.GetString(sr.Payload, 0, Proto.SRNO_CHARS);

                d.ChannelLayout = deviceMode == Mode.ThreeDp ? "DP1 + DP2 + DP3" : "DP1 + Temp + RH";

                var ck = Exchange(Proto.BuildRead(DevId, Proto.ID_DATETIME, null),
                                  Proto.DATETIME_LEN, "clock");
                if (ck != null && ck.CrcOk && ck.Payload.Length >= 12)
                {
                    string s = Encoding.ASCII.GetString(ck.Payload, 0, 12);
                    int dd, mo, yy, hh, mi, ss;
                    if (int.TryParse(s.Substring(0, 2), out dd) && int.TryParse(s.Substring(2, 2), out mo)
                     && int.TryParse(s.Substring(4, 2), out yy) && int.TryParse(s.Substring(6, 2), out hh)
                     && int.TryParse(s.Substring(8, 2), out mi) && int.TryParse(s.Substring(10, 2), out ss))
                    {
                        try { d.DeviceClock = new DateTime(2000 + yy, mo, dd, hh, mi, ss); }
                        catch (ArgumentOutOfRangeException) { }
                    }
                    d.ClockTrusted = (ck.Status & Proto.ST_RTC_INVALID) == 0;
                }
                barLogs.Value = 10; Application.DoEvents();

                // ---- every parameter the device will answer for
                foreach (var pm in Params.All)
                {
                    if (pm.Only != Mode.Unknown && deviceMode != Mode.Unknown && pm.Only != deviceMode)
                        continue;
                    var r = Exchange(Proto.BuildRead(DevId, pm.Id, null), 0, pm.Name);
                    string val;
                    if (r == null || !r.CrcOk) val = "?";
                    else if (r.InvalidPara) continue;
                    else
                    {
                        double dv;
                        val = double.TryParse(r.Text, NumberStyles.Integer,
                                              CultureInfo.InvariantCulture, out dv)
                            ? (dv / pm.Scale).ToString("F" + pm.Decimals, CultureInfo.InvariantCulture)
                            : r.Text;
                    }
                    d.Parameters.Add(new string[] {
                        pm.Group, pm.Name, "0x" + pm.Id.ToString("X2"), val, pm.Units });
                }
                barLogs.Value = 35; Application.DoEvents();

                // ---- counters
                d.LogInterval = ReadInt(0x19);
                d.RegularCount = ReadInt(Logs.ID_RDLG_CNT);
                d.RingIndex = ReadInt(Logs.ID_FLASH24_CUR);
                barLogs.Value = 45; Application.DoEvents();

                // ---- the logs themselves, straight off the port: these are streamed,
                //      so they do not go through the single request/reply exchange
                if (chkLogRam.Checked)
                {
                    d.Ram = Logs.ReadRam(port, DevId, note);
                    Say("RAM buffer: " + d.Ram.Count + " records"); Application.DoEvents();
                }
                barLogs.Value = 55; Application.DoEvents();

                if (chkLogRegular.Checked)
                {
                    int reported;
                    d.Regular = Logs.ReadRegular(port, DevId, dtFrom.Value, dtTo.Value,
                                                 note, out reported);
                    if (reported >= 0 && d.RegularCount < 0) d.RegularCount = reported;
                    Say("Regular log: " + d.Regular.Count + " records"); Application.DoEvents();
                }
                barLogs.Value = 70; Application.DoEvents();

                if (chkLogRing.Checked)
                {
                    d.Ring = Logs.ReadRing(port, DevId, (int)numRingCount.Value, note);
                    Say("24 hour ring: " + d.Ring.Count + " records"); Application.DoEvents();
                }
                barLogs.Value = 85; Application.DoEvents();

                var names = Report.ChannelNames(d.ChannelLayout);
                if (chkLogDays.Checked)
                {
                    for (int c = 0; c < 3; c++)
                        d.Days[names[c]] = Logs.ReadDays(port, DevId, c,
                                                         (int)numDayCount.Value, note);
                }
                if (chkLogMeans.Checked)
                {
                    for (int c = 0; c < 3; c++)
                        d.HourlyMeans[names[c]] = Logs.ReadHourlyMeans(port, DevId, c, note);
                }
                barLogs.Value = 100;

                lastReport = d;
                txtLogOut.Text = Report.BuildText(d).Replace("\n", Environment.NewLine);
                txtLogOut.Select(0, 0);
                Say("Report built. Use Save report and CSV to keep it.");
            });
        }

        int ReadInt(byte id)
        {
            var r = Exchange(Proto.BuildRead(DevId, id, null), 0, "counter");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return -1;
            return int.TryParse(r.Text, NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v) ? v : -1;
        }

        void SaveReport()
        {
            if (lastReport == null) { Say("Read the logs first."); return; }

            using (var dlg = new SaveFileDialog())
            {
                dlg.Filter = "Report (*.txt)|*.txt";
                dlg.FileName = "NIYAMA_" +
                    (lastReport.SerialNumber ?? "device").Trim() + "_" +
                    DateTime.Now.ToString("yyyyMMdd_HHmm") + ".txt";
                if (dlg.ShowDialog(this) != DialogResult.OK) return;

                try
                {
                    File.WriteAllText(dlg.FileName, Report.BuildText(lastReport));
                    string csv = Path.ChangeExtension(dlg.FileName, ".csv");
                    File.WriteAllText(csv, Report.BuildCsv(lastReport));
                    Say("Saved " + Path.GetFileName(dlg.FileName)
                        + " and " + Path.GetFileName(csv));
                }
                catch (Exception ex) { Say("Could not save: " + ex.Message); }
            }
        }

        // ------------------------------------------------ log tab

        TabPage LogTab()
        {
            var tp = new TabPage("Log") { Padding = new Padding(8) };
            txtLog = new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
                                   ScrollBars = ScrollBars.Vertical, Font = new Font("Consolas", 8.5f),
                                   BackColor = Color.White };
            var bar = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 34 };
            var bClear = new Button { Text = "Clear", Width = 76 };
            bClear.Click += delegate { txtLog.Clear(); };
            chkHexLog = new CheckBox { Text = "Log every frame in hex", Checked = false,
                                       Width = 170, Padding = new Padding(10, 4, 0, 0) };
            bar.Controls.Add(bClear); bar.Controls.Add(chkHexLog);
            tp.Controls.Add(txtLog);
            tp.Controls.Add(bar);
            return tp;
        }

        // ------------------------------------------------ serial plumbing

        void RefreshPorts()
        {
            string keep = cboPort.SelectedItem as string;
            cboPort.Items.Clear();
            cboPort.Items.AddRange(SerialPort.GetPortNames()
                                             .OrderBy(n => n.Length).ThenBy(n => n).ToArray());
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
                port.ReadTimeout = 500;
                port.WriteTimeout = 500;
                port.Open();
                link = new Link(port, Log, delegate { return chkHexLog.Checked; });
                btnConnect.Text = "Disconnect";
                lblLink.Text = "Connected on " + port.PortName + " at " + port.BaudRate;
                lblLink.ForeColor = Color.ForestGreen;
                Say("Connected.");
                Identify();
                if (chkPoll.Checked) pollTimer.Start();
            }
            catch (Exception ex) { Say("Could not open the port: " + ex.Message); port = null; }
        }

        void Disconnect()
        {
            pollTimer.Stop();
            try { if (port != null && port.IsOpen) port.Close(); } catch { }
            port = null;
            link = null;
            btnConnect.Text = "Connect";
            lblLink.Text = "Disconnected";
            lblLink.ForeColor = Color.Firebrick;
        }

        bool Ready { get { return link != null && link.Ready; } }

        Proto.Response Exchange(byte[] frame, int expectedLen, string what)
        {
            if (link == null || !link.Ready) { Say("Not connected."); return null; }
            return link.Exchange(frame, expectedLen, what);
        }

        /// Wraps an operation so the one-second poll cannot interleave its frames with
        /// a read or write the user just asked for.
        void Serialised(Action body)
        {
            if (busy) return;
            busy = true;
            bool polling = pollTimer.Enabled;
            pollTimer.Stop();
            try { body(); }
            finally
            {
                busy = false;
                if (polling && Ready) pollTimer.Start();
            }
        }

        void Log(string s)
        {
            if (txtLog.TextLength > 300000) txtLog.Clear();
            txtLog.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + s + Environment.NewLine);
        }

        void Say(string s) { lblStatus.Text = s; }

        byte DevId { get { return (byte)numDevId.Value; } }

        // ------------------------------------------------ operations

        void Identify()
        {
            Serialised(delegate
            {
                var r = Exchange(Proto.BuildRead(DevId, Proto.ID_SFVER, null), 0, "Firmware version");
                if (r != null && r.CrcOk && !r.InvalidPara)
                {
                    int v;
                    lblFw.Text = int.TryParse(r.Text, out v)
                        ? string.Format("{0}.{1}.{2}", v / 100, (v / 10) % 10, v % 10)
                        : r.Text;
                }
                else lblFw.Text = "?";

                // The two firmware builds lay the middle channels out differently and the
                // real-time frame carries no flag saying which.  The temperature-unit
                // parameter exists only in the Temp/RH build, so asking for it settles it.
                var t = Exchange(Proto.BuildRead(DevId, Proto.ID_TMUNIT, null), 0, "Channel layout");
                if (t != null && t.CrcOk)
                {
                    deviceMode = t.InvalidPara ? Mode.ThreeDp : Mode.TempRh;
                    ApplyMode();
                }
                ReadClockInner();

                var sr = Exchange(Proto.BuildRead(DevId, Proto.ID_SRNO, null),
                                  Proto.SRNO_LEN, "Serial number");
                if (sr != null && sr.CrcOk && sr.Payload.Length >= Proto.SRNO_CHARS)
                    txtSrNo.Text = Encoding.ASCII.GetString(sr.Payload, 0, Proto.SRNO_CHARS);
            });
        }

        void ApplyMode()
        {
            lblMode.Text = deviceMode == Mode.TempRh ? "DP1 + Temp + RH"
                         : deviceMode == Mode.ThreeDp ? "DP1 + DP2 + DP3" : "-";

            if (grid != null)
            {
                grid.CurrentCell = null;
                foreach (DataGridViewRow row in grid.Rows)
                {
                    var p = row.Tag as Param;
                    row.Visible = p == null || p.Only == Mode.Unknown
                               || deviceMode == Mode.Unknown || p.Only == deviceMode;
                }
            }

            // Only DP1 exists on a Temp/RH unit, so the channel pickers collapse to it.
            bool three = deviceMode != Mode.TempRh;
            foreach (var c in new[] { cboLimitCh, cboSlotCh, cboOffCh })
            {
                if (c == null) continue;
                if (!three) c.SelectedIndex = 0;
                c.Enabled = three;
            }
        }

        void ReadClock() { Serialised(ReadClockInner); }

        void ReadClockInner()
        {
            var r = Exchange(Proto.BuildRead(DevId, Proto.ID_DATETIME, null),
                             Proto.DATETIME_LEN, "Device clock");
            if (r == null || !r.CrcOk || r.Payload.Length < 12) return;

            string s = Encoding.ASCII.GetString(r.Payload, 0, 12);   // DDMMYYHHMMSS
            int dd, mo, yy, hh, mi, ss;
            if (!int.TryParse(s.Substring(0, 2), out dd) || !int.TryParse(s.Substring(2, 2), out mo)
             || !int.TryParse(s.Substring(4, 2), out yy) || !int.TryParse(s.Substring(6, 2), out hh)
             || !int.TryParse(s.Substring(8, 2), out mi) || !int.TryParse(s.Substring(10, 2), out ss))
            { lblRtc.Text = "?"; return; }

            try
            {
                lblRtc.Text = new DateTime(2000 + yy, mo, dd, hh, mi, ss)
                                  .ToString("dd-MMM-yyyy  HH:mm:ss");
            }
            catch (ArgumentOutOfRangeException) { lblRtc.Text = s + "  (not a valid date)"; }

            lblRtc.ForeColor = (r.Status & Proto.ST_RTC_INVALID) != 0 ? Color.Firebrick : Color.Black;
        }

        void SyncClock()
        {
            if (MessageBox.Show("Set the device clock to this PC's current time?"
                                + Environment.NewLine + Environment.NewLine
                                + DateTime.Now.ToString("dd-MMM-yyyy  HH:mm:ss")
                                + Environment.NewLine + Environment.NewLine
                                + "The firmware refuses a time earlier than the one it already "
                                + "holds, so a device running ahead cannot be pulled back this way.",
                                "Set device clock", MessageBoxButtons.OKCancel,
                                MessageBoxIcon.Question) != DialogResult.OK) return;

            Serialised(delegate
            {
                string payload = DateTime.Now.ToString("ddMMyyHHmmss", CultureInfo.InvariantCulture);
                var r = Exchange(Proto.BuildWrite(DevId, Proto.ID_DATETIME, payload), 0, "Set clock");
                if (r != null && r.CrcOk) { Say("Clock sent - reading it back."); ReadClockInner(); }
                else Say("Could not set the clock.");
            });
        }

        void PollLive()
        {
            if (!Ready || busy) return;

            var r = Exchange(Proto.BuildRead(DevId, Proto.ID_REALTIME, null),
                             Proto.REALTIME_LEN, "Live values");
            if (r == null || !r.CrcOk || r.Payload.Length < 44) return;

            var p = r.Payload;                          // payload[0] is frame byte 5
            uint epoch = BitConverter.ToUInt32(p, 0);
            lblRtc.Text = new DateTime(1970, 1, 1).AddSeconds(epoch).ToString("dd-MMM-yyyy  HH:mm:ss");
            lblRtc.ForeColor = (r.Status & Proto.ST_RTC_INVALID) != 0 ? Color.Firebrick : Color.Black;

            bool tempRh = deviceMode != Mode.ThreeDp;
            string[] names = tempRh
                ? new[] { "DP1   (Pa)", "Temperature", "Humidity   (%RH)" }
                : new[] { "DP1   (Pa)", "DP2   (Pa)", "DP3   (Pa)" };

            float[] now  = { F(p, 5),  F(p, 9),  F(p, 13) };
            float[] mins = { F(p, 17), F(p, 25), F(p, 33) };
            float[] maxs = { F(p, 21), F(p, 29), F(p, 37) };
            byte[] alm   = { p[41], p[42], p[43] };
            byte faults  = p[4];

            for (int i = 0; i < 3; i++)
            {
                bool faulty = (i == 0 && (faults & Proto.ST_DP1_FAULT) != 0)
                           || (tempRh && i > 0 && (faults & Proto.ST_RH_TM_FAULT) != 0)
                           || (!tempRh && i == 1 && (faults & Proto.ST_DP2_FAULT) != 0)
                           || (!tempRh && i == 2 && (faults & Proto.ST_DP3_FAULT) != 0);

                lblChan[i].Text = names[i];
                lblVal[i].Text = faulty ? "fault" : Fmt(now[i]);
                lblMin[i].Text = Fmt(mins[i]);
                lblMax[i].Text = Fmt(maxs[i]);
                lblAlm[i].Text = alm[i] == 1 ? "HIGH" : alm[i] == 2 ? "LOW" : "-";

                lblVal[i].ForeColor = faulty ? Color.DarkOrange
                                    : alm[i] != 0 ? Color.Firebrick : Color.Black;
                lblAlm[i].ForeColor = alm[i] != 0 ? Color.Firebrick : Color.Gray;
            }
            Say("Live - " + Proto.StatusText(r.Status));
        }

        static float F(byte[] b, int off) { return BitConverter.ToSingle(b, off); }

        static string Fmt(float v)
        {
            if (float.IsNaN(v) || float.IsInfinity(v)) return "-";
            return v.ToString("0.0", CultureInfo.InvariantCulture);
        }

        void ReadParams(List<DataGridViewRow> rows)
        {
            Serialised(delegate
            {
                int ok = 0, bad = 0;
                foreach (var row in rows)
                {
                    var p = row.Tag as Param;
                    if (p == null || p.Access == Acc.W) continue;

                    var r = Exchange(Proto.BuildRead(DevId, p.Id, null), 0, p.Group + " " + p.Name);
                    if (r == null || !r.CrcOk) { row.Cells["val"].Value = "?"; bad++; }
                    else if (r.InvalidPara) { row.Cells["val"].Value = "n/a"; }
                    else
                    {
                        double v;
                        row.Cells["val"].Value =
                            double.TryParse(r.Text, NumberStyles.Integer,
                                            CultureInfo.InvariantCulture, out v)
                                ? (v / p.Scale).ToString("F" + p.Decimals, CultureInfo.InvariantCulture)
                                : r.Text;
                        ok++;
                    }
                    Say("Reading " + p.Name + "...");
                    Application.DoEvents();
                }
                Say(string.Format("Read {0} parameter(s){1}.", ok,
                                  bad > 0 ? ", " + bad + " with no reply" : ""));
            });
        }

        void WriteParams(List<DataGridViewRow> rows)
        {
            var writable = rows.Where(r => r.Tag is Param && ((Param)r.Tag).Access != Acc.R).ToList();
            if (writable.Count == 0) { Say("Nothing writable is selected."); return; }

            var names = string.Join(Environment.NewLine,
                writable.Select(r => "    " + ((Param)r.Tag).Group + " / " + ((Param)r.Tag).Name
                                   + "   =   " + r.Cells["val"].Value));
            if (MessageBox.Show("Write these values to the device?" + Environment.NewLine
                                + Environment.NewLine + names,
                                "Confirm write", MessageBoxButtons.OKCancel,
                                MessageBoxIcon.Warning) != DialogResult.OK) return;

            Serialised(delegate
            {
                foreach (var row in writable)
                {
                    var p = (Param)row.Tag;
                    var cell = row.Cells["val"].Value;
                    if (cell == null || cell.ToString().Trim().Length == 0) continue;

                    double v;
                    if (!double.TryParse(cell.ToString().Trim(), NumberStyles.Float,
                                         CultureInfo.InvariantCulture, out v))
                    { Say("'" + cell + "' is not a number - stopped."); return; }

                    long wire = (long)Math.Round(v * p.Scale);
                    if (!Proto.FitsOnWire(wire))
                    {
                        Say(p.Name + ": " + cell + " is outside the range the device can "
                            + "accept - nothing was written.");
                        return;
                    }
                    string payload = Proto.Field5(wire);

                    // A device-ID change only takes effect on the second write within ten
                    // seconds; the first one just arms it.
                    int repeats = (p.Id == Proto.ID_DVCID) ? 2 : 1;
                    Proto.Response r = null;
                    for (int k = 0; k < repeats; k++)
                        r = Exchange(Proto.BuildWrite(DevId, p.Id, payload), 0, "Write " + p.Name);

                    if (r == null || !r.CrcOk) { Say("Write failed for " + p.Name + "."); return; }
                    if (r.InvalidPara) { Say("The device rejected " + p.Name + "."); return; }

                    if (p.Id == Proto.ID_DVCID)
                        numDevId.Value = Math.Max(0, Math.Min(250, wire));
                    if (p.Id == Proto.ID_BAUD)
                    {
                        Say("Baud code written - the device has already switched. Reconnect at the new rate.");
                        return;
                    }
                    Application.DoEvents();
                }
                Say("Write complete.");
            });

            ReadParams(writable);
        }

        // ---- indexed parameters

        void ReadLimit()
        {
            Serialised(delegate
            {
                var r = Exchange(Proto.BuildRead(DevId, Proto.ID_DP_LIMIT,
                                                 cboLimitCh.SelectedIndex.ToString()), 0, "DP limit");
                int v;
                if (Good(r) && int.TryParse(r.Text, NumberStyles.Integer,
                                            CultureInfo.InvariantCulture, out v))
                {
                    //The wire carries tenths of a Pa; the box shows Pa.
                    decimal pa = v / 10m;
                    numLimitVal.Value = Math.Max(numLimitVal.Minimum,
                                                 Math.Min(numLimitVal.Maximum, pa));
                    Say("DP clamp is " + pa.ToString("0.0", CultureInfo.InvariantCulture) + " Pa.");
                }
                else Say("Could not read the DP clamp.");
            });
        }

        void WriteLimit()
        {
            Serialised(delegate
            {
                int wire = (int)Math.Round(numLimitVal.Value * 10m);
                string payload = cboLimitCh.SelectedIndex.ToString() + Proto.Field5(wire);
                var r = Exchange(Proto.BuildWrite(DevId, Proto.ID_DP_LIMIT, payload), 0, "DP clamp");
                Say(Good(r) ? "DP clamp written." : "Could not write the DP clamp.");
            });
        }

        void ReadSlots()
        {
            Serialised(delegate
            {
                for (int slot = 0; slot < slotGrid.Rows.Count; slot++)
                {
                    string payload = cboSlotCh.SelectedIndex.ToString() + slot.ToString();
                    var r = Exchange(Proto.BuildRead(DevId, Proto.ID_DP_SLOT_OFFSET, payload),
                                     0, "Slot " + slot);
                    int v;
                    bool good = Good(r) && int.TryParse(r.Text, NumberStyles.Integer,
                                                        CultureInfo.InvariantCulture, out v);
                    v = good ? int.Parse(r.Text, CultureInfo.InvariantCulture) : 0;

                    //Held in tenths on the wire, shown in Pa.
                    slotGrid.Rows[slot].Cells[1].Value = good
                        ? (v / 10m).ToString("0.0", CultureInfo.InvariantCulture) : "?";
                    slotGrid.Rows[slot].Cells[1].Style.ForeColor =
                        good ? Color.Black : Color.Firebrick;
                    Application.DoEvents();
                }
                Say("Slot offsets read for " + cboSlotCh.Text + ".");
            });
        }

        void WriteSlots()
        {
            Serialised(delegate
            {
                for (int slot = 0; slot < slotGrid.Rows.Count; slot++)
                {
                    var cell = slotGrid.Rows[slot].Cells[1].Value;
                    decimal pa;
                    if (cell == null
                        || !decimal.TryParse(cell.ToString().Trim(), NumberStyles.Float,
                                             CultureInfo.InvariantCulture, out pa))
                    { Say("Slot " + slot + " does not hold a number - stopped."); return; }

                    int v = (int)Math.Round(pa * 10m);      // Pa in, tenths out
                    if (v < -5000 || v > 5000)
                    {
                        Say("Slot " + slot + " is outside the \u00b1500.0 Pa the device "
                            + "accepts - stopped.");
                        return;
                    }

                    // The value field is five characters wide with the sign inside it, so a
                    // negative offset carries only four digits.
                    string payload = cboSlotCh.SelectedIndex.ToString() + slot.ToString()
                                   + Proto.Field5(v);

                    var r = Exchange(Proto.BuildWrite(DevId, Proto.ID_DP_SLOT_OFFSET, payload),
                                     0, "Slot " + slot);
                    if (!Good(r)) { Say("Slot " + slot + " was rejected - stopped."); return; }
                    Application.DoEvents();
                }
                Say("Slot offsets written.");
            });

            ReadSlots();
        }

        void Unlock()
        {
            Serialised(delegate
            {
                bool factory = chkFactory.Checked;
                byte id = factory ? Proto.ID_CAL_FPWD : Proto.ID_CAL_CPWD;
                var r = Exchange(Proto.BuildWrite(DevId, id, Proto.Field5((int)numCalPwd.Value)),
                                 0, "Calibration unlock");
                if (Good(r))
                {
                    // The device does not report whether the password was right, only that
                    // the write arrived - a calibration read is what actually proves it.
                    calUnlockedAt = DateTime.UtcNow;
                    calFactory = factory;
                    Say((factory ? "Factory" : "Customer") + " unlock sent - calibration is "
                        + "open for about 60 seconds, and each calibration exchange restarts it.");
                }
                else
                {
                    calUnlockedAt = DateTime.MinValue;
                    Say("The unlock was not accepted.");
                }
            });
        }

        /// How long a calibration reply should be.  It carries the stored correction on
        /// its own when calibration is locked, and the calibration date history behind
        /// it when it is open - a different amount for each mode.
        int CalReplyLength()
        {
            if ((DateTime.UtcNow - calUnlockedAt).TotalSeconds > 55) return Proto.CAL_LEN_LOCKED;
            return calFactory ? Proto.CAL_LEN_FACTORY : Proto.CAL_LEN_CUSTOMER;
        }

        void ReadCal(byte id, Label show, string name)
        {
            Serialised(delegate { ReadCalInner(id, show, name); });
        }

        void ReadCalInner(byte id, Label show, string name)
        {
            int expect = CalReplyLength();
            var r = Exchange(Proto.BuildRead(DevId, id, null), expect, name + " calibration");

            // If the window closed on the device while this end still thought it was
            // open - or the other way round - the length is wrong and nothing parses.
            // Fall back to finding the terminator.
            if (r == null || !r.CrcOk)
            {
                if (expect != Proto.CAL_LEN_LOCKED) calUnlockedAt = DateTime.MinValue;
                r = Exchange(Proto.BuildRead(DevId, id, null), 0, name + " calibration, retry");
            }

            int v;
            if (Good(r) && r.Text.Length >= 5
                && int.TryParse(r.Text.Substring(0, 5), NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v))
            {
                show.Text = (v / 10.0).ToString("0.0", CultureInfo.InvariantCulture);
                show.ForeColor = v == 0 ? Color.DimGray : Color.Black;
                Say(name + " correction is " + show.Text
                    + (v == 0 ? " - nothing stored, or calibration is locked." : "."));
            }
            else
            {
                show.Text = "?";
                Say("Could not read the " + name.ToLower() + " calibration.");
            }
        }

        void WriteCal(byte id, NumericUpDown reference, Label show, string name)
        {
            if ((DateTime.UtcNow - calUnlockedAt).TotalSeconds > 55)
            {
                Say("Unlock calibration first - the device only accepts this while it is open.");
                return;
            }

            int tenths = (int)Math.Round(reference.Value * 10m);
            if (!Proto.FitsOnWire(tenths))
            { Say("That reference value is outside the range the device can accept."); return; }

            if (MessageBox.Show(
                    "Calibrate " + name.ToLower() + " against a reading of "
                    + reference.Value.ToString("0.0", CultureInfo.InvariantCulture) + "?"
                    + Environment.NewLine + Environment.NewLine
                    + "The device will take this as the true value and store the difference "
                    + "from what it is measuring right now. Its recorded minimum and maximum "
                    + "are reset at the same time.",
                    "Confirm calibration", MessageBoxButtons.OKCancel,
                    MessageBoxIcon.Warning) != DialogResult.OK) return;

            Serialised(delegate
            {
                var w = Exchange(Proto.BuildWrite(DevId, id, Proto.Field5(tenths)),
                                 0, "Calibrate " + name);
                if (!Good(w)) { Say("The " + name.ToLower() + " calibration was not accepted."); return; }

                calUnlockedAt = DateTime.UtcNow;       // the device restarts its own window
                Say(name + " calibrated - reading the stored correction back.");
                ReadCalInner(id, show, name);
            });
        }

        void ReadSerial()
        {
            Serialised(delegate
            {
                var r = Exchange(Proto.BuildRead(DevId, Proto.ID_SRNO, null),
                                 Proto.SRNO_LEN, "Serial number");
                if (r == null || !r.CrcOk || r.Payload.Length < Proto.SRNO_CHARS)
                { Say("Could not read the serial number."); return; }

                txtSrNo.Text = Encoding.ASCII.GetString(r.Payload, 0, Proto.SRNO_CHARS);
                Say("Serial number read.");
            });
        }

        void WriteSerial()
        {
            string s = txtSrNo.Text;
            if (s.Length != Proto.SRNO_CHARS)
            {
                Say("The serial number must be exactly " + Proto.SRNO_CHARS
                    + " characters - this one is " + s.Length + ".");
                return;
            }
            foreach (char c in s)
                if (c < 0x20 || c > 0x7E)
                { Say("The serial number must be plain printable ASCII."); return; }

            if (MessageBox.Show("Write the serial number \"" + s + "\" to the device?"
                                + Environment.NewLine + Environment.NewLine
                                + "This replaces the one already stored.",
                                "Confirm serial number", MessageBoxButtons.OKCancel,
                                MessageBoxIcon.Warning) != DialogResult.OK) return;

            Serialised(delegate
            {
                var w = Exchange(Proto.BuildWrite(DevId, Proto.ID_SRNO, s), 0, "Serial number");
                if (!Good(w)) { Say("The serial number was not accepted."); return; }
                Say("Serial number written - reading it back.");

                var r = Exchange(Proto.BuildRead(DevId, Proto.ID_SRNO, null),
                                 Proto.SRNO_LEN, "Serial number");
                if (r != null && r.CrcOk && r.Payload.Length >= Proto.SRNO_CHARS)
                {
                    string back = Encoding.ASCII.GetString(r.Payload, 0, Proto.SRNO_CHARS);
                    txtSrNo.Text = back;
                    Say(back == s ? "Serial number written and verified."
                                  : "Written, but it reads back as \"" + back + "\".");
                }
            });
        }

        void ReadOffset()
        {
            Serialised(delegate
            {
                var r = Exchange(Proto.BuildRead(DevId, Proto.ID_DP_OFFSET,
                                                 cboOffCh.SelectedIndex.ToString()), 0, "Zero offset");
                int v;
                if (Good(r) && int.TryParse(r.Text, NumberStyles.Integer,
                                            CultureInfo.InvariantCulture, out v))
                {
                    //The wire carries hundredths of a Pa; the box shows Pa.
                    decimal pa = v / 100m;
                    numOffVal.Value = Math.Max(numOffVal.Minimum, Math.Min(numOffVal.Maximum, pa));
                    Say("Zero offset is " + pa.ToString("0.00", CultureInfo.InvariantCulture) + " Pa.");
                }
                else Say("Could not read the zero offset.");
            });
        }

        void WriteOffset()
        {
            Serialised(delegate
            {
                // Entered in Pa, sent in hundredths, with the sign in a byte of its own
                // ahead of a five digit field.
                int v = (int)Math.Round(numOffVal.Value * 100m);
                if (!Proto.FitsOnWire(v))
                { Say("That offset is outside the range the device can hold."); return; }

                string payload = cboOffCh.SelectedIndex.ToString() + Proto.SignedField5(v);
                var r = Exchange(Proto.BuildWrite(DevId, Proto.ID_DP_OFFSET, payload), 0, "Zero offset");
                Say(Good(r) ? "Zero offset written."
                            : "Could not write the zero offset - is calibration still unlocked?");
            });
        }

        static bool Good(Proto.Response r) { return r != null && r.CrcOk && !r.InvalidPara; }

        static decimal Clamp(int v, NumericUpDown n)
        { return Math.Max(n.Minimum, Math.Min(n.Maximum, v)); }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}
