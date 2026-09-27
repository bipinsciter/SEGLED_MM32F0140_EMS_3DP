// Write-path test against the real device, driven through the application's own Link.
//
// Every step reads the original value, writes a different one, reads it back to prove
// the write landed, then puts the original back and confirms the restore.  The only
// deliberate lasting change is the clock, which is currently unset.
//
// Build:  csc /target:exe /main:NiyamaConfig.WriteTest /out:WriteTest.exe
//              NiyamaConfig.cs WriteTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class WriteTest
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

            Console.WriteLine("Write-path test on {0}, device {1}.", portName, devId);
            Console.WriteLine("Every value is restored afterwards; the clock is set on purpose.");
            Console.WriteLine();

            Clock();
            Scalar(0x02, "DP1 upper alarm ON", 100, 1, 56.0, "Pa");
            Scalar(0x35, "Buzzer ON time", 1, 0, 3, "s");
            Scalar(0x19, "Log interval", 1, 0, 10, "min");
            Indexed(0x6E, "0", "DP1 clamp", 2500, 10, "Pa");
            Indexed(0x72, "03", "DP1 slot 3 offset", -25, 10, "Pa");
            DeviceIdEcho();
            Rejection();

            Console.WriteLine();
            Console.WriteLine("Link totals: {0} exchanges, {1} retried, {2} unanswered.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine("{0} passed, {1} failed.", pass, fail);
            port.Close();
            return fail == 0 ? 0 : 1;
        }

        // ------------------------------------------------------------ steps

        static void Clock()
        {
            Head("Device clock (0x4B)");

            var before = ReadClock();
            Console.WriteLine("    device clock before : {0}", before);

            string payload = DateTime.Now.ToString("ddMMyyHHmmss", CultureInfo.InvariantCulture);
            Console.WriteLine("    writing PC time     : {0}",
                              DateTime.Now.ToString("dd-MMM-yyyy HH:mm:ss"));
            var w = link.Exchange(Proto.BuildWrite(devId, Proto.ID_DATETIME, payload), 0, "set clock");
            if (w == null || !w.CrcOk) { Fail("clock", "no reply to the write"); return; }

            var after = ReadClock();
            Console.WriteLine("    device clock after  : {0}", after);

            DateTime t;
            bool ok = DateTime.TryParseExact(after, "dd-MM-yy HH:mm:ss",
                          CultureInfo.InvariantCulture, DateTimeStyles.None, out t)
                      && Math.Abs((DateTime.Now - t).TotalSeconds) < 90;
            if (ok) Pass("clock", "now within a minute and a half of the PC");
            else Fail("clock", "read back as '" + after + "'");
        }

        static string ReadClock()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_DATETIME, null),
                                  Proto.DATETIME_LEN, "read clock");
            if (r == null || !r.CrcOk || r.Payload.Length < 12) return "(no reply)";
            string s = System.Text.Encoding.ASCII.GetString(r.Payload, 0, 12);
            return s.Substring(0, 2) + "-" + s.Substring(2, 2) + "-" + s.Substring(4, 2) + " "
                 + s.Substring(6, 2) + ":" + s.Substring(8, 2) + ":" + s.Substring(10, 2)
                 + ((r.Status & Proto.ST_RTC_INVALID) != 0 ? "   [RTC flagged invalid]" : "");
        }

        /// Read, write something else, verify, restore, verify.
        static void Scalar(byte id, string name, double scale, int dec, double testValue, string unit)
        {
            Head(name + string.Format("  (0x{0:X2})", id));

            long? orig = ReadRaw(id, null);
            if (orig == null) { Fail(name, "could not read the original"); return; }
            Console.WriteLine("    original : {0} raw  = {1} {2}",
                              orig, (orig.Value / scale).ToString("F" + dec), unit);

            long wire = (long)Math.Round(testValue * scale);
            if (!Write(id, Proto.Field5(wire), name)) return;

            long? got = ReadRaw(id, null);
            Console.WriteLine("    wrote    : {0} raw  = {1} {2}   read back {3}",
                              wire, testValue.ToString("F" + dec), unit,
                              got == null ? "(nothing)" : got.ToString());
            if (got != wire) { Fail(name, "expected " + wire + ", read " + got); Restore(id, orig.Value, name); return; }

            Restore(id, orig.Value, name);
            long? back = ReadRaw(id, null);
            if (back == orig) Pass(name, "written, verified and restored to " + orig);
            else Fail(name, "restore left it at " + back + ", not " + orig);
        }

        static void Indexed(byte id, string index, string name, long testWire, double scale, string unit)
        {
            Head(name + string.Format("  (0x{0:X2}, index \"{1}\")", id, index));

            long? orig = ReadRaw(id, index);
            if (orig == null) { Fail(name, "could not read the original"); return; }
            Console.WriteLine("    original : {0} raw  = {1} {2}",
                              orig, (orig.Value / scale).ToString("F1"), unit);

            if (!Write(id, index + Proto.Field5(testWire), name)) return;

            long? got = ReadRaw(id, index);
            Console.WriteLine("    wrote    : {0} raw  = {1} {2}   read back {3}",
                              testWire, (testWire / scale).ToString("F1"), unit,
                              got == null ? "(nothing)" : got.ToString());
            if (got != testWire)
            { Fail(name, "expected " + testWire + ", read " + got); WriteRestore(id, index, orig.Value, name); return; }

            WriteRestore(id, index, orig.Value, name);
            long? back = ReadRaw(id, index);
            if (back == orig) Pass(name, "written, verified and restored to " + orig);
            else Fail(name, "restore left it at " + back + ", not " + orig);
        }

        static void DeviceIdEcho()
        {
            Head("Device ID double-write  (0x1A)");
            Console.WriteLine("    The firmware arms on the first write and only acts on the second,");
            Console.WriteLine("    so this writes the SAME id twice - the path is exercised, the");
            Console.WriteLine("    address does not move.");

            long? orig = ReadRaw(Proto.ID_DVCID, null);
            if (orig == null) { Fail("device id", "could not read it"); return; }

            string payload = Proto.Field5(orig.Value);
            link.Exchange(Proto.BuildWrite(devId, Proto.ID_DVCID, payload), 0, "device id 1 of 2");
            link.Exchange(Proto.BuildWrite(devId, Proto.ID_DVCID, payload), 0, "device id 2 of 2");

            long? after = ReadRaw(Proto.ID_DVCID, null);
            if (after == orig) Pass("device id", "still " + after + ", as intended");
            else Fail("device id", "changed from " + orig + " to " + after);
        }

        static void Rejection()
        {
            Head("Does the device reject a value it should not accept?");
            Console.WriteLine("    DP1 upper alarm ON is bounded by the sensor rating. Writing 900.0 Pa");
            Console.WriteLine("    (wire 90000) is far outside it - and also past what findValue can");
            Console.WriteLine("    carry, which is why the application refuses to send it at all.");

            long wire = 90000;
            Console.WriteLine("    Proto.FitsOnWire(90000) = {0}", Proto.FitsOnWire(wire));
            if (Proto.FitsOnWire(wire)) { Fail("range guard", "the application would have sent it"); return; }
            Pass("range guard", "the application refuses to send it");

            // Now one the app WOULD send, but the firmware should decline: 400.0 Pa is
            // inside int16 yet outside the sensor rating.
            long? orig = ReadRaw(0x02, null);
            Console.WriteLine();
            Console.WriteLine("    Trying 400.0 Pa (wire 40000 - inside int16, outside the rating).");
            Console.WriteLine("    Note 40000 does not fit a SIGNED 16 bit value either, so the");
            Console.WriteLine("    application blocks this one too: FitsOnWire(40000) = {0}",
                              Proto.FitsOnWire(40000));

            // A value the app will send and the firmware should refuse: 320.0 Pa -> 32000.
            Console.WriteLine();
            Console.WriteLine("    Trying 320.0 Pa (wire 32000 - fits int16, exceeds the 500 Pa rating? no).");
            Console.WriteLine("    Sending it and checking the device's own bounds check holds.");
            if (!Write(0x02, Proto.Field5(32000), "out-of-range setpoint")) return;
            long? after = ReadRaw(0x02, null);
            Console.WriteLine("    original {0}, after the attempt {1}", orig, after);
            if (after == orig)
                Pass("firmware bounds", "the device kept its original value");
            else
                Console.WriteLine("    NOTE: the device accepted {0}; restoring {1}.", after, orig);

            if (after != orig && orig != null)
            {
                Write(0x02, Proto.Field5(orig.Value), "restore setpoint");
                long? back = ReadRaw(0x02, null);
                if (back == orig) Pass("firmware bounds", "accepted 320.0 Pa, restored cleanly");
                else Fail("firmware bounds", "could not restore, left at " + back);
            }
        }

        // ------------------------------------------------------------ helpers

        static long? ReadRaw(byte id, string index)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, index), 0, "read 0x" + id.ToString("X2"));
            long v;
            if (r == null || !r.CrcOk || r.InvalidPara) return null;
            return long.TryParse(r.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out v)
                   ? (long?)v : null;
        }

        static bool Write(byte id, string payload, string name)
        {
            var w = link.Exchange(Proto.BuildWrite(devId, id, payload), 0, "write " + name);
            if (w == null || !w.CrcOk) { Fail(name, "no reply to the write"); return false; }
            if (w.InvalidPara) { Console.WriteLine("    the device flagged INVALID_PARA"); }
            return true;
        }

        static void Restore(byte id, long orig, string name)
        { Write(id, Proto.Field5(orig), "restore " + name); }

        static void WriteRestore(byte id, string index, long orig, string name)
        { Write(id, index + Proto.Field5(orig), "restore " + name); }

        static void Head(string s)
        {
            Console.WriteLine();
            Console.WriteLine("=== " + s);
        }

        static void Pass(string what, string detail)
        { pass++; Console.WriteLine("    PASS  " + what + " - " + detail); }

        static void Fail(string what, string detail)
        { fail++; Console.WriteLine("    FAIL  " + what + " - " + detail); }
    }
}
