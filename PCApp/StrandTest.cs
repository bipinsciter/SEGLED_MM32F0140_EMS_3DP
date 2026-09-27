// Prove the stranded-extreme fix actually runs, rather than inferring it from values
// that look reasonable.
//
// Sane extremes on their own prove nothing - a configuration reset would produce them
// too. So this recreates the exact condition that used to strand them:
//
//   1. push the reading up with a slot offset, so a large maximum is recorded
//   2. take the slot offset away again, leaving that maximum behind
//   3. narrow the clamp below it, while keeping the clamp above the live reading
//
// Step 3 is what used to be fatal. The maximum now sits outside the clamp, a maximum
// only ever moves up, and no reading can reach it again - so before the fix it stayed
// there for good. With the fix it restarts from the present reading.
//
// Everything is restored, including on failure.
//
// Build:  csc /target:exe /main:NiyamaConfig.StrandTest /out:StrandTest.exe
//              NiyamaConfig.cs StrandTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class StrandTest
    {
        static Link link;
        static byte devId = 1;
        static int pass, fail;

        // originals, restored at the end whatever happens
        static int origClamp, origSlot0;
        static bool haveOriginals;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Stranded-extreme test on {0}, device {1}.", portName, devId);
            Console.WriteLine("The clamp and DP1 slot 0 are changed and put back.");

            try { Run(); }
            finally { Restore(port); }

            Console.WriteLine();
            Console.WriteLine("Link: {0} exchanges, {1} retried, {2} unanswered.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine("{0} passed, {1} failed.", pass, fail);
            port.Close();
            return fail == 0 ? 0 : 1;
        }

        static void Run()
        {
            Head("Starting point");
            int? c = ReadRaw(Proto.ID_DP_LIMIT, "0");
            int? s0 = ReadRaw(Proto.ID_DP_SLOT_OFFSET, "00");
            if (c == null || s0 == null) { Fail("setup", "could not read the settings"); return; }
            origClamp = c.Value; origSlot0 = s0.Value; haveOriginals = true;

            var live = Live();
            Console.WriteLine("    clamp {0} Pa, slot 0 {1} Pa, live {2} Pa, min {3}, max {4}",
                              Pa(origClamp, 10), Pa(origSlot0, 10),
                              F(live.v), F(live.mn), F(live.mx));

            // ---- 1. push the reading up so a large maximum gets recorded
            Head("1. Record a large maximum, using a +50.0 Pa slot offset");
            if (!Write(Proto.ID_DP_SLOT_OFFSET, "00" + Proto.Field5(500), "slot 0")) return;
            Settle(5);
            var hi = Live();
            Console.WriteLine("    live {0} Pa, max {1} Pa", F(hi.v), F(hi.mx));
            if (hi.mx > 40f) Pass("large maximum", "recorded " + F(hi.mx) + " Pa");
            else { Fail("large maximum", "max only reached " + F(hi.mx)); return; }

            // ---- 2. take the offset away; the maximum stays behind
            Head("2. Remove the offset - the maximum should stay where it is");
            if (!Write(Proto.ID_DP_SLOT_OFFSET, "00" + Proto.Field5(origSlot0), "slot 0")) return;
            Settle(5);
            var back = Live();
            Console.WriteLine("    live {0} Pa, max {1} Pa", F(back.v), F(back.mx));
            if (Math.Abs(back.mx - hi.mx) < 0.2f)
                Pass("maximum persists", "still " + F(back.mx) + " Pa, as a maximum should");
            else Console.WriteLine("    (max moved to " + F(back.mx) + ", continuing)");

            float stranded = back.mx;

            // ---- 3. narrow the clamp below that maximum, but above the live reading
            Head("3. Narrow the clamp below the maximum - this used to strand it");
            int newClamp = 100;                          // 10.0 Pa
            Console.WriteLine("    live is {0} Pa and the maximum is {1} Pa, so a 10.0 Pa clamp",
                              F(back.v), F(stranded));
            Console.WriteLine("    leaves the reading measurable while putting the maximum");
            Console.WriteLine("    out of reach - a maximum only ever moves up.");
            if (Math.Abs(back.v) > 9.0f)
            { Fail("clamp choice", "the live reading is too large for a 10.0 Pa clamp"); return; }
            if (!Write(Proto.ID_DP_LIMIT, "0" + Proto.Field5(newClamp), "clamp")) return;
            Settle(6);

            var after = Live();
            Console.WriteLine();
            Console.WriteLine("    live {0} Pa, min {1} Pa, max {2} Pa",
                              F(after.v), F(after.mn), F(after.mx));

            if (Math.Abs(after.mx - stranded) < 0.2f)
            {
                Fail("stranded maximum", "still " + F(after.mx) + " Pa, outside the 10.0 Pa "
                     + "clamp - the fix is not in this firmware");
            }
            else if (Math.Abs(after.mx) <= 10.0f && Math.Abs(after.mx - after.v) < 2.0f)
            {
                Pass("stranded maximum", "restarted from the live reading: "
                     + F(stranded) + " -> " + F(after.mx) + " Pa");
            }
            else
            {
                Fail("stranded maximum", "moved to " + F(after.mx)
                     + ", which is neither the old value nor the live reading");
            }

            if (Math.Abs(after.mn) <= 10.0f)
                Pass("minimum", "inside the clamp at " + F(after.mn) + " Pa");
            else
                Fail("minimum", F(after.mn) + " Pa is outside the 10.0 Pa clamp");
        }

        static void Restore(SerialPort port)
        {
            if (!haveOriginals) return;
            Head("Restoring");
            Write(Proto.ID_DP_LIMIT, "0" + Proto.Field5(origClamp), "clamp");
            Write(Proto.ID_DP_SLOT_OFFSET, "00" + Proto.Field5(origSlot0), "slot 0");
            int? c = ReadRaw(Proto.ID_DP_LIMIT, "0");
            int? s = ReadRaw(Proto.ID_DP_SLOT_OFFSET, "00");
            Console.WriteLine("    clamp back to {0} Pa, slot 0 back to {1} Pa",
                              c == null ? "?" : Pa(c.Value, 10),
                              s == null ? "?" : Pa(s.Value, 10));
            if (c == origClamp && s == origSlot0) Pass("restore", "both settings are back");
            else Fail("restore", "settings did not come back");

            Settle(4);
            var l = Live();
            Console.WriteLine("    live {0} Pa, min {1} Pa, max {2} Pa",
                              F(l.v), F(l.mn), F(l.mx));
            Console.WriteLine("    (the extremes now restart from readings taken since the test)");
        }

        // ---------------------------------------------------------------- helpers

        struct Snap { public float v, mn, mx; }

        static Snap Live()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                  Proto.REALTIME_LEN, "live");
            var s = new Snap();
            if (r == null || !r.CrcOk || r.Payload.Length < 44)
            { s.v = s.mn = s.mx = float.NaN; return s; }
            s.v = BitConverter.ToSingle(r.Payload, 5);
            s.mn = BitConverter.ToSingle(r.Payload, 17);
            s.mx = BitConverter.ToSingle(r.Payload, 21);
            return s;
        }

        static void Settle(int seconds)
        {
            Console.Write("    waiting {0} s ", seconds);
            for (int i = 0; i < seconds; i++)
            { System.Threading.Thread.Sleep(1000); Console.Write("."); }
            Console.WriteLine();
        }

        static int? ReadRaw(byte id, string index)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, index), 0, "read");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return null;
            return int.TryParse(r.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out v)
                   ? (int?)v : null;
        }

        static bool Write(byte id, string payload, string what)
        {
            var w = link.Exchange(Proto.BuildWrite(devId, id, payload), 0, "write " + what);
            if (w == null || !w.CrcOk) { Fail(what, "no reply to the write"); return false; }
            if (w.InvalidPara) { Fail(what, "device flagged INVALID_PARA"); return false; }
            return true;
        }

        static string Pa(int raw, int scale)
        { return (raw / (decimal)scale).ToString("0.0", CultureInfo.InvariantCulture); }

        static string F(float v)
        { return float.IsNaN(v) ? "?" : v.ToString("0.00", CultureInfo.InvariantCulture); }

        static void Head(string s)
        { Console.WriteLine(); Console.WriteLine("=== " + s); }

        static void Pass(string w, string d) { pass++; Console.WriteLine("    PASS  " + w + " - " + d); }
        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
