// Verify the parameter-word write path, using the same protocol calls the
// application's Enabled parameters group makes.
//
// The word is written through 0x4E with the factory password and the device restarts
// a few seconds later, so each step has to wait for it to come back.
//
// The test clears the alerts bit and puts it back. That is a real, visible change and
// the least disruptive one available: leaving the clock bit alone means logging keeps
// running throughout, which is itself worth confirming across a restart.
//
// Build:  csc /target:exe /main:NiyamaConfig.ParamWordTest /out:ParamWordTest.exe
//              NiyamaConfig.cs Logs.cs Report.cs ParamWordTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class ParamWordTest
    {
        const int BIT_DP1 = 0x0001, BIT_CLOCK = 0x0002, BIT_ALERT = 0x0004;
        const int BIT_RH = 0x0020, BIT_TEMP = 0x0040;

        static Link link;
        static SerialPort port;
        static byte devId = 1;
        static int pass, fail;
        static int original = -1;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);

            port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 700; port.WriteTimeout = 700;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Parameter-word write test on {0}, device {1}.", portName, devId);
            Console.WriteLine("The alerts bit is cleared and then put back; the clock bit is");
            Console.WriteLine("left alone so logging keeps running throughout.");

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
            original = ReadWord();
            if (original < 0) { Fail("read", "could not read the parameter word"); return; }
            Console.WriteLine("    word {0}  (0x{0:X4})   {1}", original, Describe(original));
            Pass("read", "the word reads back as " + original);

            int before = LogCount();
            Console.WriteLine("    regular log holds {0} record(s)", before);

            // ---- clear the alerts bit
            int changed = original & ~BIT_ALERT;
            if (changed == original)
            {
                Console.WriteLine();
                Console.WriteLine("    Alerts are already off, so the test sets them instead.");
                changed = original | BIT_ALERT;
            }

            Head("Writing " + changed + "  (0x" + changed.ToString("X4") + ")");
            Console.WriteLine("    {0}", Describe(changed));
            if (!Write(changed)) return;

            Console.WriteLine("    waiting for the restart");
            WaitForDevice(20);

            int got = ReadWord();
            Console.WriteLine("    reads back {0}  (0x{0:X4})   {1}", got, Describe(got));
            if (got == changed) Pass("write", "the device took the new word");
            else Fail("write", "expected " + changed + ", reads " + got);

            // ---- the bit really is off, and nothing else moved
            if ((got & BIT_ALERT) != (changed & BIT_ALERT))
                Fail("alerts bit", "did not change as asked");
            else Pass("alerts bit", "changed as asked");

            if ((got & ~BIT_ALERT) == (original & ~BIT_ALERT))
                Pass("other bits", "untouched");
            else Fail("other bits", "something else moved: " + Describe(got));
        }

        static void Restore()
        {
            if (original < 0) return;
            Head("Restoring " + original);
            if (!Write(original)) { Fail("restore", "the write was not accepted"); return; }
            WaitForDevice(20);

            int got = ReadWord();
            Console.WriteLine("    reads back {0}  (0x{0:X4})   {1}", got, Describe(got));
            if (got == original) Pass("restore", "back to " + original);
            else Fail("restore", "left at " + got);

            // ---- and logging picks up again after two restarts
            Console.WriteLine();
            Console.WriteLine("    Checking logging survived both restarts. RTCSetFlag lives in");
            Console.WriteLine("    EEPROM, so a restart alone should not stop the logs.");
            int a = LogCount();
            Console.WriteLine("    regular log holds {0} record(s); waiting 70 s", a);
            System.Threading.Thread.Sleep(70000);
            int b = LogCount();
            Console.WriteLine("    now {0} record(s)", b);
            if (b > a) Pass("logging", "still advancing after the restarts");
            else Fail("logging", "stopped at " + b + " - the clock may need setting again");
        }

        // ---------------------------------------------------------------- helpers

        static string Describe(int w)
        {
            if (w < 0) return "?";
            var on = new System.Collections.Generic.List<string>();
            if ((w & BIT_DP1) != 0) on.Add("DP1");
            if ((w & BIT_CLOCK) != 0) on.Add("Clock");
            if ((w & BIT_ALERT) != 0) on.Add("Alerts");
            if ((w & BIT_RH) != 0) on.Add("Humidity");
            if ((w & BIT_TEMP) != 0) on.Add("Temperature");
            return on.Count == 0 ? "nothing on" : string.Join(", ", on.ToArray());
        }

        static int ReadWord()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_PARAM_WORD,
                                                  Proto.FACTORY_PARASET_PWD), 0, "word");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return -1;
            return int.TryParse(r.Text, NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v) ? v : -1;
        }

        static bool Write(int w)
        {
            var r = link.Exchange(Proto.BuildWrite(devId, Proto.ID_PARAM_WORD,
                                      Proto.FACTORY_PARASET_PWD + w.ToString("D5")),
                                  0, "word");
            if (r == null || !r.CrcOk) { Fail("write", "no reply"); return false; }
            if (r.InvalidPara) { Fail("write", "INVALID_PARA"); return false; }
            return true;
        }

        static int LogCount()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Logs.ID_RDLG_CNT, null), 0, "count");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return -1;
            return int.TryParse(r.Text, NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v) ? v : -1;
        }

        /// The device drops off the bus while it restarts; poll until it answers again.
        static void WaitForDevice(int seconds)
        {
            var until = DateTime.UtcNow.AddSeconds(seconds);
            while (DateTime.UtcNow < until)
            {
                System.Threading.Thread.Sleep(1500);
                var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_SFVER, null), 0, "ping");
                if (r != null && r.CrcOk && !r.InvalidPara)
                {
                    Console.WriteLine("    back after {0:0} s",
                                      seconds - (until - DateTime.UtcNow).TotalSeconds);
                    return;
                }
            }
            Console.WriteLine("    still not answering after {0} s", seconds);
        }

        static void Head(string s) { Console.WriteLine(); Console.WriteLine("=== " + s); }
        static void Pass(string w, string d) { pass++; Console.WriteLine("    PASS  " + w + " - " + d); }
        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
