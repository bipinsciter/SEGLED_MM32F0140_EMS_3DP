// Build a real report from the device, using the same Logs and Report code the
// application ships, so what is verified here is what the Logs tab does.
//
// READ ONLY.
//
// Build:  csc /target:exe /main:NiyamaConfig.ReportTest /out:ReportTest.exe
//              NiyamaConfig.cs Logs.cs Report.cs ReportTest.cs

using System;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Text;

namespace NiyamaConfig
{
    public static class ReportTest
    {
        static Link link;
        static SerialPort port;
        static byte devId = 1;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            string outPath = argv.Length > 2 ? argv[2] : null;

            port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 900; port.WriteTimeout = 900;
            port.Open();
            link = new Link(port, delegate { }, delegate { return false; });

            Action<string> note = delegate(string s) { Console.WriteLine("   . " + s); };
            var d = new ReportData { DeviceId = devId };

            Console.WriteLine("Collecting identity and clock...");
            int iv;
            var v = link.Exchange(Proto.BuildRead(devId, Proto.ID_SFVER, null), 0, "version");
            if (Good(v) && int.TryParse(v.Text, out iv))
                d.FirmwareVersion = string.Format("{0}.{1}.{2}", iv / 100, (iv / 10) % 10, iv % 10);

            var sr = link.Exchange(Proto.BuildRead(devId, Proto.ID_SRNO, null),
                                   Proto.SRNO_LEN, "serial");
            if (sr != null && sr.CrcOk && sr.Payload.Length >= Proto.SRNO_CHARS)
                d.SerialNumber = Encoding.ASCII.GetString(sr.Payload, 0, Proto.SRNO_CHARS);

            var tu = link.Exchange(Proto.BuildRead(devId, Proto.ID_TMUNIT, null), 0, "layout");
            d.ChannelLayout = (tu != null && tu.CrcOk && tu.InvalidPara)
                              ? "DP1 + DP2 + DP3" : "DP1 + Temp + RH";

            var ck = link.Exchange(Proto.BuildRead(devId, Proto.ID_DATETIME, null),
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

            var fw = link.Exchange(Proto.BuildRead(devId, 0x4E, "1234"), 0, "feature word");
            int fwv;
            if (Good(fw) && int.TryParse(fw.Text, out fwv)) d.FeatureWord = fwv;

            Console.WriteLine("Reading settings...");
            var mode = d.ChannelLayout.Contains("Temp") ? Mode.TempRh : Mode.ThreeDp;
            foreach (var pm in Params.All)
            {
                if (pm.Only != Mode.Unknown && pm.Only != mode) continue;
                var r = link.Exchange(Proto.BuildRead(devId, pm.Id, null), 0, pm.Name);
                if (r == null || !r.CrcOk || r.InvalidPara) continue;
                double dv;
                string val = double.TryParse(r.Text, NumberStyles.Integer,
                                             CultureInfo.InvariantCulture, out dv)
                    ? (dv / pm.Scale).ToString("F" + pm.Decimals, CultureInfo.InvariantCulture)
                    : r.Text;
                d.Parameters.Add(new string[] { pm.Group, pm.Name,
                                                "0x" + pm.Id.ToString("X2"), val, pm.Units });
            }
            Console.WriteLine("   {0} settings read", d.Parameters.Count);

            d.LogInterval = ReadInt(0x19);
            d.RegularCount = ReadInt(Logs.ID_RDLG_CNT);
            d.RingIndex = ReadInt(Logs.ID_FLASH24_CUR);
            Console.WriteLine("   interval {0} min, regular {1}, ring {2}",
                              d.LogInterval, d.RegularCount, d.RingIndex);

            Console.WriteLine("Reading the RAM buffer...");
            d.Ram = Logs.ReadRam(port, devId, note);
            Console.WriteLine("   {0} records", d.Ram.Count);

            Console.WriteLine("Reading the regular log for today...");
            int reported;
            d.Regular = Logs.ReadRegular(port, devId, DateTime.Now.Date,
                                         DateTime.Now.AddMinutes(5), note, out reported);
            Console.WriteLine("   {0} records (device reported {1})", d.Regular.Count, reported);

            Console.WriteLine("Reading the 24 hour ring, newest 20...");
            d.Ring = Logs.ReadRing(port, devId, 20, note);
            Console.WriteLine("   {0} records", d.Ring.Count);

            var names = Report.ChannelNames(d.ChannelLayout);
            Console.WriteLine("Reading the 15 day archive and the hourly means...");
            for (int c = 0; c < 3; c++)
            {
                d.Days[names[c]] = Logs.ReadDays(port, devId, c, 15, note);
                d.HourlyMeans[names[c]] = Logs.ReadHourlyMeans(port, devId, c, note);
                Console.WriteLine("   {0}: {1} day(s), {2} hourly mean(s)",
                                  names[c], d.Days[names[c]].Count, d.HourlyMeans[names[c]].Count);
            }

            string text = Report.BuildText(d);
            string csv = Report.BuildCsv(d);

            if (outPath != null)
            {
                File.WriteAllText(outPath, text);
                File.WriteAllText(Path.ChangeExtension(outPath, ".csv"), csv);
                Console.WriteLine();
                Console.WriteLine("Wrote {0}", outPath);
                Console.WriteLine("Wrote {0}", Path.ChangeExtension(outPath, ".csv"));
                Console.WriteLine("CSV data rows: {0}",
                                  csv.Split('\n').Length - 2);
            }
            else Console.WriteLine(text);

            port.Close();
            return 0;
        }

        static int ReadInt(byte id)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, null), 0, "counter");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return -1;
            return int.TryParse(r.Text, NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v) ? v : -1;
        }

        static bool Good(Proto.Response r)
        { return r != null && r.CrcOk && !r.InvalidPara; }
    }
}
