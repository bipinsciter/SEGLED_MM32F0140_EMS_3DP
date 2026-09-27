// Verify the FACTORY temperature calibration path while the display is in Fahrenheit.
//
// Same arithmetic as the customer path: the reference is an absolute temperature in
// the displayed unit, the difference the device derives is a SPAN, and a span converts
// by the 1.8 scale with no 32 degree offset.
//
// Restoring this one is harder than the customer path. DFLT_CAL_ID (0x52) clears only
// the customer correction; nothing but a fresh-device boot clears the factory one. So
// the restore calibrates back to the original value instead, in Celsius where the
// arithmetic is exact integer (stored = measured - reference), iterating until it
// lands. A factory calibration also zeroes the customer correction, so the test
// refuses to run unless both are already zero.
//
// Build:  csc /target:exe /main:NiyamaConfig.FactCalTest /out:FactCalTest.exe
//              NiyamaConfig.cs FactCalTest.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class FactCalTest
    {
        const byte ID_CAL_FPWD = 0x37;
        const int FACTORY_PASSWORD = 1000;

        static Link link;
        static byte devId = 1;
        static int pass, fail;
        static int custPwd = 100;

        static int origUnit = -1;
        static bool touched;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Factory temperature calibration test on {0}, device {1}.",
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
            Head("Which firmware is running");
            var v = ReadNum(Proto.ID_SFVER);
            Console.WriteLine("    reports {0}", v);
            if (v != null && v >= 106) Pass("version", "1.0.6 or later");
            else { Fail("version", "need 1.0.6, this is " + v); return; }

            // ---- both corrections must be clear, because a factory calibration
            //      wipes the customer one as a side effect
            Head("Calibration state before the test");
            if (!UnlockFactory()) return;
            var fac0 = ReadCal(Proto.CAL_LEN_FACTORY);
            if (!UnlockCustomer()) return;
            var cus0 = ReadCal(Proto.CAL_LEN_CUSTOMER);
            Console.WriteLine("    factory correction  {0} tenths", Show(fac0));
            Console.WriteLine("    customer correction {0} tenths", Show(cus0));

            if (fac0 == null || cus0 == null) { Fail("read", "could not read both"); return; }
            if (fac0 != 0 || cus0 != 0)
            {
                Fail("precondition", "a calibration is already stored (factory " + fac0
                     + ", customer " + cus0 + "). This test will not overwrite it, because "
                     + "a factory calibration also clears the customer one.");
                return;
            }
            Pass("precondition", "both corrections are zero, safe to proceed");

            // ---- Fahrenheit
            Head("Switching the display to Fahrenheit");
            origUnit = ReadNum(Proto.ID_TMUNIT) ?? -1;
            Console.WriteLine("    unit was {0}", origUnit == 1 ? "Fahrenheit" : "Celsius");
            if (origUnit != 1)
            {
                if (!Write(Proto.ID_TMUNIT, Proto.Field5(1), "unit")) return;
                System.Threading.Thread.Sleep(700);
            }
            if (ReadNum(Proto.ID_TMUNIT) == 1) Pass("unit", "display is in Fahrenheit");
            else { Fail("unit", "could not switch"); return; }

            // ---- calibrate 1.8 F low
            Head("Factory calibration against a reference 1.8 F below the reading");
            float c0 = LiveC();
            float f0 = c0 * 1.8f + 32.0f;
            int refTenthsF = (int)Math.Round((f0 - 1.8f) * 10f);
            Console.WriteLine("    device reads {0} C = {1} F; telling it the truth is {2} F",
                              F(c0), F(f0), (refTenthsF / 10m).ToString("0.0", CultureInfo.InvariantCulture));
            Console.WriteLine("    1.8 F of span is 1.0 C, so the correction should be +10 tenths");

            if (!UnlockFactory()) return;
            touched = true;
            if (!Write(Proto.ID_TMCAL, Proto.Field5(refTenthsF), "factory calibration")) return;
            System.Threading.Thread.Sleep(1200);

            if (!UnlockFactory()) return;
            var fac1 = ReadCal(Proto.CAL_LEN_FACTORY);
            Console.WriteLine();
            Console.WriteLine("    factory correction now {0} tenths", Show(fac1));
            if (fac1 != null && Math.Abs(fac1.Value - 10) <= 2)
                Pass("correction", fac1 + " tenths, i.e. 1.0 C as intended");
            else
                Fail("correction", "expected about 10, got " + Show(fac1)
                     + "   (1.0.5 stored 64 here)");

            float c1 = LiveC();
            Console.WriteLine("    device now reads {0} C, was {1} C, moved {2} C",
                              F(c1), F(c0), F(c1 - c0));
            if (Math.Abs((c0 - c1) - 1.0f) < 0.35f)
                Pass("applied", "the reading fell by 1.0 C, matching the correction");
            else
                Fail("applied", "moved " + F(c1 - c0) + " C, expected -1.0");

            // ---- the documented side effect
            if (!UnlockCustomer()) return;
            var cus1 = ReadCal(Proto.CAL_LEN_CUSTOMER);
            Console.WriteLine("    customer correction is {0} tenths", Show(cus1));
            if (cus1 == 0) Pass("side effect", "customer correction cleared, as the firmware intends");
            else Fail("side effect", "customer correction is " + Show(cus1) + ", expected 0");

            // ---- unit must not disturb it
            Head("Switching the unit to Celsius and back");
            if (!Write(Proto.ID_TMUNIT, Proto.Field5(0), "unit to C")) return;
            System.Threading.Thread.Sleep(700);
            if (!UnlockFactory()) return;
            var facC = ReadCal(Proto.CAL_LEN_FACTORY);
            Console.WriteLine("    in Celsius it reads {0} tenths", Show(facC));

            if (!Write(Proto.ID_TMUNIT, Proto.Field5(1), "unit to F")) return;
            System.Threading.Thread.Sleep(700);
            if (!UnlockFactory()) return;
            var facF = ReadCal(Proto.CAL_LEN_FACTORY);
            Console.WriteLine("    back in Fahrenheit it reads {0} tenths", Show(facF));

            if (facC == fac1 && facF == fac1)
                Pass("unit change", "the correction survived both switches unchanged");
            else
                Fail("unit change", "it moved: " + Show(fac1) + " -> " + Show(facC)
                     + " -> " + Show(facF));
        }

        // ---------------------------------------------------------------- restore

        static void Restore()
        {
            if (!touched) { RestoreUnit(); return; }

            Head("Restoring the factory correction to zero");
            Console.WriteLine("    Nothing but a fresh-device boot clears this one, so it is");
            Console.WriteLine("    calibrated back. In Celsius the firmware does");
            Console.WriteLine("    stored = measured - reference in whole tenths, so sending the");
            Console.WriteLine("    measured value drives it to zero; it iterates for sensor drift.");

            // Celsius, where the arithmetic is exact
            Write(Proto.ID_TMUNIT, Proto.Field5(0), "unit to C");
            System.Threading.Thread.Sleep(800);

            int reference = 0;
            for (int attempt = 1; attempt <= 6; attempt++)
            {
                if (!UnlockFactory()) break;
                var cur = ReadCal(Proto.CAL_LEN_FACTORY);
                if (cur == null) { Console.WriteLine("    attempt {0}: no reply", attempt); continue; }
                if (cur == 0)
                {
                    Pass("restore factory", "back to zero after " + (attempt - 1) + " calibration(s)");
                    break;
                }

                float live = LiveC();
                if (float.IsNaN(live)) { Console.WriteLine("    attempt {0}: no live value", attempt); continue; }

                // The reading already has the correction applied, so the raw value the
                // firmware will difference against is live + correction.
                if (attempt == 1)
                    reference = (int)Math.Round((live + cur.Value / 10f) * 10f);
                else
                    reference = reference + cur.Value;      // stored = ss1 - reference

                Console.WriteLine("    attempt {0}: correction {1}, live {2} C, sending reference {3}",
                                  attempt, cur, F(live), reference);
                if (!UnlockFactory()) break;
                Write(Proto.ID_TMCAL, Proto.Field5(reference), "restore calibration");
                System.Threading.Thread.Sleep(1200);

                if (attempt == 6)
                {
                    if (!UnlockFactory()) break;
                    var last = ReadCal(Proto.CAL_LEN_FACTORY);
                    if (last == 0) Pass("restore factory", "back to zero");
                    else Fail("restore factory", "left at " + Show(last)
                              + " tenths - tell the operator, it is a "
                              + (last == null ? "?" : (last.Value / 10m).ToString("0.0"))
                              + " C offset");
                }
            }

            RestoreUnit();
        }

        static void RestoreUnit()
        {
            if (origUnit != 0 && origUnit != 1) return;
            Write(Proto.ID_TMUNIT, Proto.Field5(origUnit), "unit");
            System.Threading.Thread.Sleep(700);
            var u = ReadNum(Proto.ID_TMUNIT);
            Console.WriteLine("    unit is back to {0}", u == 1 ? "Fahrenheit" : "Celsius");
            if (u == origUnit) Pass("restore unit", "as it was");
            else Fail("restore unit", "reads " + u);
        }

        // ---------------------------------------------------------------- helpers

        static bool UnlockFactory()
        {
            var r = link.Exchange(Proto.BuildWrite(devId, ID_CAL_FPWD,
                                                   Proto.Field5(FACTORY_PASSWORD)), 0, "factory unlock");
            if (r != null && r.CrcOk && !r.InvalidPara) return true;
            Fail("factory unlock", "not accepted");
            return false;
        }

        static bool UnlockCustomer()
        {
            var r = link.Exchange(Proto.BuildWrite(devId, Proto.ID_CAL_CPWD,
                                                   Proto.Field5(custPwd)), 0, "customer unlock");
            if (r != null && r.CrcOk && !r.InvalidPara) return true;
            Fail("customer unlock", "not accepted");
            return false;
        }

        static int? ReadCal(int expect)
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_TMCAL, null), expect, "correction");
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

        static float LiveC()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                  Proto.REALTIME_LEN, "live");
            if (r == null || !r.CrcOk || r.Payload.Length < 44) return float.NaN;
            return BitConverter.ToSingle(r.Payload, 9);
        }

        static int? ReadNum(byte id)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, null), 0, "read");
            int v;
            if (r == null || !r.CrcOk || r.InvalidPara) return null;
            return int.TryParse(r.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out v)
                   ? (int?)v : null;
        }

        static bool Write(byte id, string payload, string what)
        {
            var w = link.Exchange(Proto.BuildWrite(devId, id, payload), 0, "write " + what);
            if (w == null || !w.CrcOk) { Fail(what, "no reply"); return false; }
            if (w.InvalidPara) { Fail(what, "INVALID_PARA"); return false; }
            return true;
        }

        static string Show(int? v) { return v == null ? "?" : v.Value.ToString(); }
        static string F(float v)
        { return float.IsNaN(v) ? "?" : v.ToString("0.00", CultureInfo.InvariantCulture); }

        static void Head(string s) { Console.WriteLine(); Console.WriteLine("=== " + s); }
        static void Pass(string w, string d) { pass++; Console.WriteLine("    PASS  " + w + " - " + d); }
        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
