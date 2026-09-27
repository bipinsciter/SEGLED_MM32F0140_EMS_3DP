// Watch the DP minimum and maximum against the live reading.
//
// The real-time frame carries the value and both extremes in one shot, so polling it
// shows exactly which reading moved an extreme - and whether the extreme moved to a
// value the frame never reported.
//
// READ ONLY.
//
// Build:  csc /target:exe /main:NiyamaConfig.MinMax /out:MinMax.exe
//              NiyamaConfig.cs MinMax.cs
// Run:    MinMax.exe COM3 1 [seconds]

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class MinMax
    {
        static Link link;
        static byte devId = 1;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            int seconds = argv.Length > 2 ? int.Parse(argv[2]) : 60;

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("    . " + s); },
                            delegate { return false; });

            Console.WriteLine("DP extremes watch on {0}, device {1}, {2} s. Read only.",
                              portName, devId, seconds);
            Console.WriteLine();

            State();

            Console.WriteLine();
            Console.WriteLine("Polling the live frame once a second. Only changes are printed.");
            Console.WriteLine("{0,-9} {1,10} {2,10} {3,10}   {4}",
                              "time", "DP1", "min", "max", "note");

            float lastMin = float.NaN, lastMax = float.NaN;
            float seenLo = float.MaxValue, seenHi = float.MinValue;
            var start = DateTime.Now;
            int n = 0;

            while ((DateTime.Now - start).TotalSeconds < seconds)
            {
                var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                      Proto.REALTIME_LEN, "live");
                if (r == null || !r.CrcOk || r.Payload.Length < 44)
                { System.Threading.Thread.Sleep(900); continue; }

                var p = r.Payload;
                float v = BitConverter.ToSingle(p, 5);
                float mn = BitConverter.ToSingle(p, 17);
                float mx = BitConverter.ToSingle(p, 21);
                n++;

                if (v < seenLo) seenLo = v;
                if (v > seenHi) seenHi = v;

                bool changed = !(mn.Equals(lastMin) && mx.Equals(lastMax));
                if (changed || n == 1)
                {
                    string note = "";
                    if (n > 1)
                    {
                        if (!mn.Equals(lastMin))
                            note += string.Format("min {0} -> {1}{2}  ", F(lastMin), F(mn),
                                       Math.Abs(mn - v) > 0.05f ? "  NOT the live value" : "");
                        if (!mx.Equals(lastMax))
                            note += string.Format("max {0} -> {1}{2}", F(lastMax), F(mx),
                                       Math.Abs(mx - v) > 0.05f ? "  NOT the live value" : "");
                    }
                    Console.WriteLine("{0,-9} {1,10} {2,10} {3,10}   {4}",
                                      DateTime.Now.ToString("HH:mm:ss"), F(v), F(mn), F(mx), note);
                    lastMin = mn; lastMax = mx;
                }
                System.Threading.Thread.Sleep(900);
            }

            Console.WriteLine();
            Console.WriteLine("Over {0} samples the live reading spanned {1} to {2}.",
                              n, F(seenLo), F(seenHi));
            Console.WriteLine("The device reports its extremes as {0} and {1}.",
                              F(lastMin), F(lastMax));
            // An extreme that simply predates this run is not a fault - a lifetime
            // minimum is expected to sit below whatever the last 25 seconds happened to
            // show. The fault is an extreme the device can no longer reach at all,
            // which is one lying outside the configured clamp.
            if (clampPa > 0 && (Math.Abs(lastMin) > clampPa || Math.Abs(lastMax) > clampPa))
            {
                Console.WriteLine();
                Console.WriteLine("STRANDED: an extreme lies outside the {0} Pa clamp, so no",
                                  clampPa.ToString("0.0", CultureInfo.InvariantCulture));
                Console.WriteLine("reading can ever supersede it. Firmware 1.0.5 restarts such an");
                Console.WriteLine("extreme from the present reading; older builds leave it there.");
            }
            else
            {
                Console.WriteLine("Both extremes are inside the clamp, so either can still move.");
            }
            port.Close();
            return 0;
        }

        static float clampPa;

        static void State()
        {
            Console.WriteLine("Current settings that bear on the extremes:");
            clampPa = ShowIdx(Proto.ID_DP_LIMIT, "0", "DP1 clamp", 10.0, "Pa");
            ShowIdx(Proto.ID_DP_OFFSET, "0", "DP1 zero offset", 100.0, "Pa");
            for (int slot = 0; slot < 6; slot++)
                ShowIdx(Proto.ID_DP_SLOT_OFFSET, "0" + slot, "DP1 slot " + slot, 10.0, "Pa");
            Show(0x1C, "DP1 minimum", 100.0, "Pa");
            Show(0x1D, "DP1 maximum", 100.0, "Pa");
            Console.WriteLine();
            Console.WriteLine("  (an extreme outside the clamp can never be superseded)");
        }

        static void Show(byte id, string name, double scale, string unit)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, null), 0, name);
            Report(r, name, scale, unit);
        }


        static float ShowIdx(byte id, string index, string name, double scale, string unit)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, index), 0, name);
            return Report(r, name, scale, unit);
        }

        static float Report(Proto.Response r, string name, double scale, string unit)
        {
            int v;
            if (r == null || !r.CrcOk) { Console.WriteLine("  {0,-18} no reply", name); return 0f; }
            if (r.InvalidPara) { Console.WriteLine("  {0,-18} not in build", name); return 0f; }
            if (!int.TryParse(r.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out v))
            { Console.WriteLine("  {0,-18} \"{1}\"", name, r.Text); return 0f; }
            Console.WriteLine("  {0,-18} raw {1,8}   = {2} {3}",
                              name, v, (v / scale).ToString("0.00", CultureInfo.InvariantCulture), unit);
            return (float)(v / scale);
        }

        static string F(float v)
        {
            if (float.IsNaN(v)) return "-";
            if (v == float.MaxValue || v == float.MinValue) return "-";
            return v.ToString("0.00", CultureInfo.InvariantCulture);
        }
    }
}
