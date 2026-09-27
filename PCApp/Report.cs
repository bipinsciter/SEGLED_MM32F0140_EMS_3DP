// Building a report from the device: identity, clock, every settable parameter, and
// whichever logs were read.
//
// Two files come out of one run. The report is meant to be read by a person, so it
// leads with who the instrument is and what its clock says - a log is worth little
// without knowing whether the clock behind its timestamps was trustworthy. The CSV is
// the same records in a form a spreadsheet can take.
//
// Temperature is written in Celsius throughout, whatever the display is set to,
// because that is what the records hold. The display unit at the time is recorded
// alongside rather than used to convert.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace NiyamaConfig
{
    class ReportData
    {
        public string FirmwareVersion = "?", SerialNumber = "?";
        public int DeviceId = -1;
        public DateTime? DeviceClock;
        public bool ClockTrusted;
        public DateTime TakenAt = DateTime.Now;
        public string ChannelLayout = "?";
        public int FeatureWord = -1;

        public readonly List<string[]> Parameters = new List<string[]>();  // group, name, id, value, unit

        public int RegularCount = -1, RingIndex = -1, LogInterval = -1;
        public List<LogRecord> Regular = new List<LogRecord>();
        public List<LogRecord> Ring = new List<LogRecord>();
        public List<LogRecord> Ram = new List<LogRecord>();
        public readonly Dictionary<string, List<DayRecord>> Days = new Dictionary<string, List<DayRecord>>();
        public readonly Dictionary<string, List<float>> HourlyMeans = new Dictionary<string, List<float>>();
    }

    static class Report
    {
        public static string[] ChannelNames(string layout)
        {
            return layout.Contains("Temp")
                ? new[] { "DP1 (Pa)", "Temperature (C)", "Humidity (%RH)" }
                : new[] { "DP1 (Pa)", "DP2 (Pa)", "DP3 (Pa)" };
        }

        // ---------------------------------------------------------------- text

        public static string BuildText(ReportData d)
        {
            var s = new StringBuilder();
            var ch = ChannelNames(d.ChannelLayout);

            Rule(s, '=');
            s.AppendLine("NIYAMA_3DP  DATA REPORT");
            Rule(s, '=');
            s.AppendLine();

            s.AppendLine("Instrument");
            s.AppendLine("  Serial number      " + d.SerialNumber);
            s.AppendLine("  Device address     " + (d.DeviceId < 0 ? "?" : d.DeviceId.ToString()));
            s.AppendLine("  Firmware           " + d.FirmwareVersion);
            s.AppendLine("  Channels           " + d.ChannelLayout);
            if (d.FeatureWord >= 0)
                s.AppendLine("  Feature word       " + d.FeatureWord
                             + "  (0x" + d.FeatureWord.ToString("X4") + ")");
            s.AppendLine();

            s.AppendLine("Clock");
            s.AppendLine("  Device clock       " + (d.DeviceClock == null
                            ? "not readable"
                            : d.DeviceClock.Value.ToString("dd-MMM-yyyy HH:mm:ss")));
            s.AppendLine("  This PC            " + d.TakenAt.ToString("dd-MMM-yyyy HH:mm:ss"));
            if (d.DeviceClock != null)
            {
                double drift = (d.TakenAt - d.DeviceClock.Value).TotalSeconds;
                s.AppendLine("  Difference         " + drift.ToString("0") + " s");
            }
            s.AppendLine("  Clock trusted      " + (d.ClockTrusted ? "yes" : "NO"));
            if (!d.ClockTrusted)
            {
                s.AppendLine();
                s.AppendLine("  The device reports its clock as untrustworthy. Logging stops while");
                s.AppendLine("  that is so, and any timestamps below predate the fault. Treat the");
                s.AppendLine("  times in this report with suspicion until the clock is set again.");
            }
            s.AppendLine();

            // ---- parameters
            Rule(s, '-');
            s.AppendLine("SETTINGS");
            Rule(s, '-');
            string lastGroup = null;
            foreach (var p in d.Parameters)
            {
                if (p[0] != lastGroup) { s.AppendLine(); s.AppendLine("  " + p[0]); lastGroup = p[0]; }
                s.AppendLine(string.Format("    {0,-30} {1,-5} {2,12} {3}", p[1], p[2], p[3], p[4]));
            }
            s.AppendLine();

            // ---- logs
            Rule(s, '-');
            s.AppendLine("LOGS");
            Rule(s, '-');
            s.AppendLine();
            s.AppendLine("  Log interval                     "
                         + (d.LogInterval < 0 ? "?" : d.LogInterval + " min"));
            s.AppendLine("  Records in the regular log       "
                         + (d.RegularCount < 0 ? "?" : d.RegularCount.ToString()));
            s.AppendLine("  Position in the 24 hour ring     "
                         + (d.RingIndex < 0 ? "?" : d.RingIndex + " of 1440"));
            s.AppendLine();

            Section(s, "Regular log", d.Regular, ch);
            Section(s, "24 hour ring", d.Ring, ch);
            Section(s, "RAM buffer, most recent readings", d.Ram, ch);

            foreach (var kv in d.Days)
            {
                s.AppendLine();
                s.AppendLine("  15 day archive - " + kv.Key);
                if (kv.Value.Count == 0)
                {
                    s.AppendLine("    nothing stored yet; a day has to roll over before the first entry");
                }
                else
                {
                    s.AppendLine(string.Format("    {0,-12} {1,10} {2,10} {3,10}",
                                               "day", "minimum", "maximum", "mean"));
                    foreach (var r in kv.Value)
                        s.AppendLine(string.Format("    {0,-12} {1,10} {2,10} {3,10}",
                            r.Day == null ? "?" : r.Day.Value.ToString("dd-MMM-yy"),
                            F(r.Min), F(r.Max), F(r.Mean)));
                }
            }

            foreach (var kv in d.HourlyMeans)
            {
                s.AppendLine();
                s.AppendLine("  24 hourly means - " + kv.Key);
                if (kv.Value.Count == 0) { s.AppendLine("    nothing returned"); continue; }
                for (int h = 0; h < kv.Value.Count; h++)
                {
                    if (h % 6 == 0) { s.AppendLine(); s.Append("    "); }
                    s.Append(string.Format("{0:00}h {1,9}   ", h, F(kv.Value[h])));
                }
                s.AppendLine();
            }

            s.AppendLine();
            Rule(s, '=');
            s.AppendLine("Temperatures are in Celsius throughout: that is what the records hold,");
            s.AppendLine("whatever unit the display was set to. Where a record was taken while the");
            s.AppendLine("display was in Fahrenheit the F column says so, but the value is not");
            s.AppendLine("converted.");
            Rule(s, '=');
            return s.ToString();
        }

        static void Section(StringBuilder s, string title, List<LogRecord> recs, string[] ch)
        {
            s.AppendLine();
            s.AppendLine("  " + title + "  (" + recs.Count + " record"
                         + (recs.Count == 1 ? "" : "s") + ")");
            if (recs.Count == 0)
            {
                s.AppendLine("    none read");
                return;
            }
            s.AppendLine(string.Format("    {0,-19} {1,10} {2,10} {3,10}  {4}  {5}",
                                       "stamp", ch[0], ch[1], ch[2], "alarms", "notes"));
            foreach (var r in recs)
            {
                var notes = new List<string>();
                if (r.RtcSet == 0) notes.Add("clock was not set");
                if (r.Faults != 0) notes.Add(Proto.StatusText(r.Faults));
                if (r.Fahrenheit) notes.Add("display in F");

                s.AppendLine(string.Format("    {0,-19} {1,10} {2,10} {3,10}  {4}  {5}",
                    r.Stamp == null ? "(no timestamp)" : r.Stamp.Value.ToString("dd-MMM-yy HH:mm:ss"),
                    F(r.V1), F(r.V2), F(r.V3),
                    A(r.Alarm1) + A(r.Alarm2) + A(r.Alarm3),
                    string.Join("; ", notes.ToArray())));
            }
        }

        // ---------------------------------------------------------------- csv

        public static string BuildCsv(ReportData d)
        {
            var ch = ChannelNames(d.ChannelLayout);
            var s = new StringBuilder();
            s.AppendLine("log,stamp,\"" + ch[0] + "\",\"" + ch[1] + "\",\"" + ch[2] + "\","
                         + "min1,max1,min2,max2,min3,max3,"
                         + "alarm1,alarm2,alarm3,clock_was_set,display_in_F,faults");

            Rows(s, "regular", d.Regular);
            Rows(s, "ring24", d.Ring);
            Rows(s, "ram", d.Ram);

            foreach (var kv in d.Days)
            {
                foreach (var r in kv.Value)
                    s.AppendLine(string.Format(CultureInfo.InvariantCulture,
                        "day_{0},{1},,,,{2},{3},,,,,,,,,,mean={4}",
                        Safe(kv.Key),
                        r.Day == null ? "" : r.Day.Value.ToString("yyyy-MM-dd"),
                        R(r.Min), R(r.Max), R(r.Mean)));
            }

            foreach (var kv in d.HourlyMeans)
            {
                for (int h = 0; h < kv.Value.Count; h++)
                    s.AppendLine(string.Format(CultureInfo.InvariantCulture,
                        "hourmean_{0},{1:00}:00,{2},,,,,,,,,,,,,,",
                        Safe(kv.Key), h, R(kv.Value[h])));
            }
            return s.ToString();
        }

        static void Rows(StringBuilder s, string tag, List<LogRecord> recs)
        {
            foreach (var r in recs)
                s.AppendLine(string.Format(CultureInfo.InvariantCulture,
                    "{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16}",
                    tag,
                    r.Stamp == null ? "" : r.Stamp.Value.ToString("yyyy-MM-dd HH:mm:ss"),
                    R(r.V1), R(r.V2), R(r.V3),
                    R(r.Min1), R(r.Max1), R(r.Min2), R(r.Max2), R(r.Min3), R(r.Max3),
                    r.Alarm1, r.Alarm2, r.Alarm3,
                    r.RtcSet != 0 ? 1 : 0, r.Fahrenheit ? 1 : 0, r.Faults));
        }

        static string Safe(string s) { return s.Replace(",", " ").Replace("\"", ""); }

        static string A(byte a) { return a == 1 ? "H" : a == 2 ? "L" : "-"; }

        static string F(float v)
        {
            if (float.IsNaN(v) || float.IsInfinity(v)) return "-";
            return v.ToString("0.00", CultureInfo.InvariantCulture);
        }

        static string R(float v)
        {
            if (float.IsNaN(v) || float.IsInfinity(v)) return "";
            return v.ToString("0.000", CultureInfo.InvariantCulture);
        }

        static void Rule(StringBuilder s, char c) { s.AppendLine(new string(c, 78)); }
    }
}
