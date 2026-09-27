// Round-trip the DP clamp and a slot offset in Pa against the real device.
//
// The application now takes Pa on the Calibration tab while the wire still carries
// tenths, so what matters is that Pa in gives the right raw value out and back again.
// Every value is restored.
//
// Build:  csc /target:exe /main:NiyamaConfig.PaTest /out:PaTest.exe
//              NiyamaConfig.cs PaTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class PaTest
    {
        static Link link;
        static byte devId = 1;
        static int pass, fail;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Pa round-trip on {0}, device {1}. Everything is restored.",
                              portName, devId);

            Clamp();
            Slot();

            Console.WriteLine();
            Console.WriteLine("Link: {0} exchanges, {1} retried, {2} unanswered.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine("{0} passed, {1} failed.", pass, fail);
            port.Close();
            return fail == 0 ? 0 : 1;
        }

        static void Clamp()
        {
            Head("DP clamp (0x6E) entered in Pa");

            int? orig = ReadRaw(Proto.ID_DP_LIMIT, "0");
            if (orig == null) { Fail("clamp", "could not read it"); return; }
            Console.WriteLine("    stored raw {0} = {1} Pa",
                              orig, (orig.Value / 10m).ToString("0.0", CultureInfo.InvariantCulture));

            // what the box would hold, and what a write of it must put on the wire
            decimal shown = orig.Value / 10m;
            if ((int)Math.Round(shown * 10m) == orig.Value)
                Pass("clamp scaling", orig + " raw shows as " + shown.ToString("0.0") + " Pa");
            else Fail("clamp scaling", "round trip changed it");

            decimal testPa = 120.0m;
            int wire = (int)Math.Round(testPa * 10m);
            Console.WriteLine("    writing {0} Pa, which must go out as raw {1}", testPa, wire);
            if (!Write(Proto.ID_DP_LIMIT, "0" + Proto.Field5(wire), "clamp")) return;

            int? got = ReadRaw(Proto.ID_DP_LIMIT, "0");
            Console.WriteLine("    device now reports raw {0} = {1} Pa", got,
                              got == null ? "?" : (got.Value / 10m).ToString("0.0"));
            if (got == wire) Pass("clamp write", "120.0 Pa stored as raw 1200");
            else Fail("clamp write", "expected " + wire + ", got " + got);

            Write(Proto.ID_DP_LIMIT, "0" + Proto.Field5(orig.Value), "restore clamp");
            int? back = ReadRaw(Proto.ID_DP_LIMIT, "0");
            if (back == orig) Pass("clamp restore", "back to raw " + orig);
            else Fail("clamp restore", "left at " + back);
        }

        static void Slot()
        {
            Head("Slot offset (0x72) entered in Pa");

            int? orig = ReadRaw(Proto.ID_DP_SLOT_OFFSET, "02");
            if (orig == null) { Fail("slot", "could not read it"); return; }
            Console.WriteLine("    DP1 slot 2 stored raw {0} = {1} Pa",
                              orig, (orig.Value / 10m).ToString("0.0", CultureInfo.InvariantCulture));

            // a negative with one decimal exercises both the scaling and the sign-inside
            // -the-field encoding at once
            decimal testPa = -3.5m;
            int wire = (int)Math.Round(testPa * 10m);
            Console.WriteLine("    writing {0} Pa -> raw {1} -> field \"{2}\"",
                              testPa, wire, Proto.Field5(wire));
            if (!Write(Proto.ID_DP_SLOT_OFFSET, "02" + Proto.Field5(wire), "slot")) return;

            int? got = ReadRaw(Proto.ID_DP_SLOT_OFFSET, "02");
            Console.WriteLine("    device now reports raw {0} = {1} Pa", got,
                              got == null ? "?" : (got.Value / 10m).ToString("0.0"));
            if (got == wire) Pass("slot write", "-3.5 Pa stored as raw -35");
            else Fail("slot write", "expected " + wire + ", got " + got);

            Write(Proto.ID_DP_SLOT_OFFSET, "02" + Proto.Field5(orig.Value), "restore slot");
            int? back = ReadRaw(Proto.ID_DP_SLOT_OFFSET, "02");
            if (back == orig) Pass("slot restore", "back to raw " + orig);
            else Fail("slot restore", "left at " + back);
        }

        static int? ReadRaw(byte id, string index)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, index), 0, "read 0x" + id.ToString("X2"));
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

        static void Head(string s)
        {
            Console.WriteLine();
            Console.WriteLine("=== " + s);
        }

        static void Pass(string w, string d) { pass++; Console.WriteLine("    PASS  " + w + " - " + d); }
        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
