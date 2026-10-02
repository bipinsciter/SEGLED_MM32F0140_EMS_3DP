// Verify the DP span factor (0x5F), using the same protocol calls the Calibration tab
// makes.
//
// It is shaped like the zero offset: channel index, then the sign in a byte of its
// own ahead of five digits, in hundredths of a Pa. The device accepts it only while
// CUSTOMER calibration is unlocked - a factory unlock will not do, so the test checks
// that too.
//
// Every value is restored.
//
// Build:  csc /target:exe /main:NiyamaConfig.SwFactTest /out:SwFactTest.exe
//              NiyamaConfig.cs Logs.cs Report.cs SwFactTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class SwFactTest
    {
        const byte ID_CAL_FPWD = 0x37;
        const int FACTORY_PASSWORD = 1000;

        static Link link;
        static byte devId = 1;
        static int pwd = 100;
        static int pass, fail;
        static int original = -1;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            if (argv.Length > 2) pwd = int.Parse(argv[2]);

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 700; port.WriteTimeout = 700;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("DP span factor test on {0}, device {1}.", portName, devId);

            try { Run(); }
            finally { Restore(); }

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
            if (!UnlockCustomer()) return;
            original = ReadRaw();
            if (original == int.MinValue) { Fail("read", "no usable reply"); return; }
            Console.WriteLine("    DP1 span factor: raw {0} = {1} Pa",
                              original, (original / 100m).ToString("0.00", CultureInfo.InvariantCulture));
            Pass("read", "reads back as " + original);

            // ---- encoding: 1.25 Pa must go out as +00125
            Head("Encoding");
            decimal pa = 1.25m;
            int wire = (int)Math.Round(pa * 100m);
            Console.WriteLine("    {0} Pa -> raw {1} -> field \"{2}\"",
                              pa, wire, Proto.SignedField5(wire));
            if (wire == 125 && Proto.SignedField5(wire) == "+00125")
                Pass("encoding", "1.25 Pa becomes +00125");
            else Fail("encoding", "got " + Proto.SignedField5(wire));

            // ---- a negative, to exercise the separate sign byte
            Head("Writing -2.50 Pa");
            int test = -250;
            if (!UnlockCustomer()) return;
            if (!Write(test)) return;
            System.Threading.Thread.Sleep(400);

            if (!UnlockCustomer()) return;
            int got = ReadRaw();
            Console.WriteLine("    reads back raw {0} = {1} Pa", got,
                              got == int.MinValue ? "?" : (got / 100m).ToString("0.00"));
            if (got == test) Pass("write", "-2.50 Pa stored as raw -250");
            else Fail("write", "expected " + test + ", got " + got);

            // ---- the customer password is the one that counts
            Head("A factory unlock must NOT be enough");
            Console.WriteLine("    The firmware gates 0x5F on bool_CustmerCalibrationOn, so");
            Console.WriteLine("    unlocking with the factory password should leave it unchanged.");
            var f = link.Exchange(Proto.BuildWrite(devId, ID_CAL_FPWD,
                                      Proto.Field5(FACTORY_PASSWORD)), 0, "factory unlock");
            if (f == null || !f.CrcOk) { Fail("factory unlock", "no reply"); return; }

            int under = -1234;
            Write(under);
            System.Threading.Thread.Sleep(400);
            if (!UnlockCustomer()) return;
            int after = ReadRaw();
            Console.WriteLine("    after the attempt: raw {0}", after);
            if (after == test)
                Pass("factory unlock rejected", "the value is untouched, as the firmware intends");
            else if (after == under)
                Fail("factory unlock rejected", "the write went through under a factory unlock");
            else
                Fail("factory unlock rejected", "unexpected value " + after);
        }

        static void Restore()
        {
            if (original == int.MinValue || original < -32768) return;
            Head("Restoring");
            if (!UnlockCustomer()) return;
            Write(original);
            System.Threading.Thread.Sleep(400);
            if (!UnlockCustomer()) return;
            int back = ReadRaw();
            Console.WriteLine("    DP1 span factor: raw {0}", back);
            if (back == original) Pass("restore", "back to " + original);
            else Fail("restore", "left at " + back);
        }

        // ---------------------------------------------------------------- helpers

        static bool UnlockCustomer()
        {
            var r = link.Exchange(Proto.BuildWrite(devId, Proto.ID_CAL_CPWD, Proto.Field5(pwd)),
                                  0, "customer unlock");
            if (r != null && r.CrcOk && !r.InvalidPara) return true;
            Fail("unlock", "the customer password was not accepted");
            return false;
        }

        static int ReadRaw()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_DP_SW_FACT, "0"),
                                  0, "span factor");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return int.MinValue;
            return int.TryParse(r.Text, NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v) ? v : int.MinValue;
        }

        static bool Write(int raw)
        {
            string payload = "0" + Proto.SignedField5(raw);
            var w = link.Exchange(Proto.BuildWrite(devId, Proto.ID_DP_SW_FACT, payload),
                                  0, "span factor");
            if (w == null || !w.CrcOk) { Fail("write", "no reply"); return false; }
            if (w.InvalidPara) { Fail("write", "INVALID_PARA"); return false; }
            return true;
        }

        static void Head(string s) { Console.WriteLine(); Console.WriteLine("=== " + s); }
        static void Pass(string w, string d) { pass++; Console.WriteLine("    PASS  " + w + " - " + d); }
        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
