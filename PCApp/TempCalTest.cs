// Verify temperature calibration while the display is in Fahrenheit.
//
// The correction is subtracted from the reading in Celsius, so it is stored in tenths
// of a degree C whatever unit is displayed. A calibration reference given in
// Fahrenheit is an absolute temperature, but the difference the device derives from it
// is a SPAN, which converts by the 1.8 scale alone.
//
// The test calibrates against a reference exactly 1.8 F below what the device reads.
// 1.8 F of span is 1.0 C, so the correct stored correction is +10 tenths and the live
// reading must fall by exactly 1.0 C. Firmware 1.0.5 stored 64 and applied -14.2 C.
//
// It then switches the unit to C and back, which must leave the correction untouched.
//
// This WRITES a calibration. It refuses to run if one is already stored, and it puts
// the unit and the calibration back afterwards.
//
// Build:  csc /target:exe /main:NiyamaConfig.TempCalTest /out:TempCalTest.exe
//              NiyamaConfig.cs TempCalTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class TempCalTest
    {
        static Link link;
        static byte devId = 1;
        static int pass, fail;
        static int pwd = 100;

        static int origUnit = -1;
        static bool calibrated;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            if (argv.Length > 2) pwd = int.Parse(argv[2]);

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Fahrenheit temperature calibration test on {0}, device {1}.",
                              portName, devId);

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
            // ---- which build
            Head("Which firmware is running");
            var v = ReadNum(Proto.ID_SFVER, null, 0);
            if (v == null) { Fail("version", "no reply"); return; }
            Console.WriteLine("    reports {0}  ->  {1}.{2}.{3}",
                              v, v / 100, (v / 10) % 10, v % 10);
            if (v >= 106) Pass("version", "1.0.6 or later, the fix should be present");
            else { Fail("version", "this is " + v + ", the fix is not in it"); return; }

            // ---- unlock and check nothing is already stored
            Head("Calibration state before the test");
            if (!Unlock()) return;
            var cal0 = ReadCal();
            if (cal0 == null) { Fail("read correction", "no usable reply"); return; }
            Console.WriteLine("    stored customer correction: {0} tenths ({1} C)",
                              cal0, (cal0.Value / 10m).ToString("0.0", CultureInfo.InvariantCulture));
            if (cal0 != 0)
            {
                Fail("precondition", "a calibration of " + cal0 + " tenths is already stored; "
                     + "this test will not overwrite it");
                return;
            }
            Pass("precondition", "no customer calibration stored, safe to proceed");

            // ---- put the display in Fahrenheit
            Head("Switching the display to Fahrenheit");
            origUnit = ReadNum(Proto.ID_TMUNIT, null, 0) ?? -1;
            Console.WriteLine("    unit was {0}", origUnit == 1 ? "Fahrenheit" : "Celsius");
            if (origUnit != 1)
            {
                if (!Write(Proto.ID_TMUNIT, Proto.Field5(1), "unit")) return;
                System.Threading.Thread.Sleep(600);
            }
            var u = ReadNum(Proto.ID_TMUNIT, null, 0);
            if (u == 1) Pass("unit", "display is in Fahrenheit");
            else { Fail("unit", "could not switch, reads " + u); return; }

            // ---- calibrate against a reference 1.8 F below the reading
            Head("Calibrating against a reference 1.8 F below the reading");
            float c0 = LiveC();
            float f0 = c0 * 1.8f + 32.0f;
            Console.WriteLine("    device reads {0} C, which is {1} F",
                              F(c0), F(f0));

            int refTenthsF = (int)Math.Round((f0 - 1.8f) * 10f);
            Console.WriteLine("    telling it the truth is {0} F  (payload \"{1}\")",
                              (refTenthsF / 10m).ToString("0.0", CultureInfo.InvariantCulture),
                              Proto.Field5(refTenthsF));
            Console.WriteLine("    1.8 F of span is 1.0 C, so the correction should be +10 tenths");

            if (!Unlock()) return;
            if (!Write(Proto.ID_TMCAL, Proto.Field5(refTenthsF), "calibration")) return;
            calibrated = true;
            System.Threading.Thread.Sleep(1200);

            var cal1 = ReadCal();
            Console.WriteLine();
            Console.WriteLine("    stored correction now: {0} tenths ({1} C)",
                              cal1,
                              cal1 == null ? "?" : (cal1.Value / 10m).ToString("0.0", CultureInfo.InvariantCulture));
            if (cal1 != null && Math.Abs(cal1.Value - 10) <= 2)
                Pass("correction", cal1 + " tenths, i.e. 1.0 C as intended");
            else
                Fail("correction", "expected about 10 tenths, got " + (cal1 == null ? "nothing" : cal1.ToString())
                     + "   (1.0.5 stored 64 here)");

            float c1 = LiveC();
            Console.WriteLine("    device now reads {0} C, was {1} C, moved {2} C",
                              F(c1), F(c0), F(c1 - c0));
            if (Math.Abs((c0 - c1) - 1.0f) < 0.35f)
                Pass("applied", "the reading fell by 1.0 C, matching the correction");
            else
                Fail("applied", "the reading moved by " + F(c1 - c0) + " C, expected -1.0");

            float f1 = c1 * 1.8f + 32.0f;
            Console.WriteLine("    in Fahrenheit that is {0} F; the operator asked for {1} F",
                              F(f1), (refTenthsF / 10m).ToString("0.0", CultureInfo.InvariantCulture));
            if (Math.Abs(f1 - refTenthsF / 10f) < 0.7f)
                Pass("reads back", "the display now shows what the operator entered");
            else
                Fail("reads back", "shows " + F(f1) + " F, expected about "
                     + (refTenthsF / 10m).ToString("0.0"));

            // ---- the unit must not disturb the stored correction
            Head("Switching the unit to Celsius and back");
            if (!Write(Proto.ID_TMUNIT, Proto.Field5(0), "unit to C")) return;
            System.Threading.Thread.Sleep(700);
            if (!Unlock()) return;
            var calC = ReadCal();
            Console.WriteLine("    in Celsius the correction reads {0} tenths", calC);

            if (!Write(Proto.ID_TMUNIT, Proto.Field5(1), "unit to F")) return;
            System.Threading.Thread.Sleep(700);
            if (!Unlock()) return;
            var calF = ReadCal();
            Console.WriteLine("    back in Fahrenheit it reads {0} tenths", calF);

            if (calC == cal1 && calF == cal1)
                Pass("unit change", "the correction survived both switches unchanged");
            else
                Fail("unit change", "it moved: " + cal1 + " -> " + calC + " -> " + calF
                     + "   (1.0.5 turned 10 into 338 and then back into 10)");
        }

        static void Restore()
        {
            Head("Restoring");
            if (calibrated)
            {
                if (Unlock())
                {
                    // index '1' is the temperature channel in the DP1+Temp+RH build
                    Write(Proto.ID_DFLT_CAL, "1", "clear calibration");
                    System.Threading.Thread.Sleep(900);
                    if (Unlock())
                    {
                        var back = ReadCal();
                        Console.WriteLine("    correction is now {0} tenths", back);
                        if (back == 0) Pass("restore calibration", "cleared back to zero");
                        else Fail("restore calibration", "left at " + back);
                    }
                }
            }

            if (origUnit == 0 || origUnit == 1)
            {
                Write(Proto.ID_TMUNIT, Proto.Field5(origUnit), "unit");
                System.Threading.Thread.Sleep(700);
                var u = ReadNum(Proto.ID_TMUNIT, null, 0);
                Console.WriteLine("    unit is back to {0}", u == 1 ? "Fahrenheit" : "Celsius");
                if (u == origUnit) Pass("restore unit", "as it was");
                else Fail("restore unit", "reads " + u + ", was " + origUnit);
            }

            Console.WriteLine("    note: a calibration resets the recorded temperature");
            Console.WriteLine("    minimum and maximum; those now restart from current readings.");
        }

        // ---------------------------------------------------------------- helpers

        static bool Unlock()
        {
            var r = link.Exchange(Proto.BuildWrite(devId, Proto.ID_CAL_CPWD, Proto.Field5(pwd)),
                                  0, "unlock");
            if (r != null && r.CrcOk && !r.InvalidPara) return true;
            Fail("unlock", "the calibration password was not accepted");
            return false;
        }

        /// While customer calibration is open the reply is 72 bytes; the correction is
        /// the first five characters of the payload.
        static int? ReadCal()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_TMCAL, null),
                                  Proto.CAL_LEN_CUSTOMER, "correction");
            if (r == null || !r.CrcOk)
            {
                r = link.Exchange(Proto.BuildRead(devId, Proto.ID_TMCAL, null), 0, "correction");
                if (r == null || !r.CrcOk) return null;
            }
            if (r.InvalidPara || r.Text.Length < 5) return null;
            int v;
            return int.TryParse(r.Text.Substring(0, 5), NumberStyles.Integer,
                                CultureInfo.InvariantCulture, out v) ? (int?)v : null;
        }

        /// The live frame carries temperature in Celsius whatever the display shows.
        static float LiveC()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                  Proto.REALTIME_LEN, "live");
            if (r == null || !r.CrcOk || r.Payload.Length < 44) return float.NaN;
            return BitConverter.ToSingle(r.Payload, 9);
        }

        static int? ReadNum(byte id, string index, int expect)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, index), expect, "read");
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

        static string F(float v)
        { return float.IsNaN(v) ? "?" : v.ToString("0.00", CultureInfo.InvariantCulture); }

        static void Head(string s)
        { Console.WriteLine(); Console.WriteLine("=== " + s); }

        static void Pass(string w, string d) { pass++; Console.WriteLine("    PASS  " + w + " - " + d); }
        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
