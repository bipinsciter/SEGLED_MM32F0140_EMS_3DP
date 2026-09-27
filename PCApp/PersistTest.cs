// Does a stored temperature calibration survive a power cycle in Fahrenheit?
//
// This is the one defect of the four that a power cycle is needed to see. Firmware
// before 1.0.6 ran the absolute C-to-F formula over the stored corrections on every
// boot while the display was in Fahrenheit, so a correction of 10 tenths came back as
// 10*1.8 + 32 = 50 - and did so again on the next boot, and the next.
//
// Zero correction in Celsius proves nothing: neither branch touches anything. So this
// stores a real correction, puts the display in Fahrenheit, and records exactly what
// the device shows. After the power cycle the same numbers have to come back.
//
//   PersistTest.exe COM3 1 arm     before the power cycle
//   PersistTest.exe COM3 1 check   after it - also restores what it changed
//
// Build:  csc /target:exe /main:NiyamaConfig.PersistTest /out:PersistTest.exe
//              NiyamaConfig.cs PersistTest.cs

using System;
using System.Globalization;
using System.IO;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class PersistTest
    {
        static Link link;
        static byte devId = 1;
        static int pass, fail;
        static int pwd = 100;

        static string StateFile
        {
            get
            {
                return Path.Combine(Path.GetTempPath(), "niyama_persist_state.txt");
            }
        }

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            string mode = argv.Length > 2 ? argv[2].ToLower() : "arm";

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Calibration persistence test on {0}, device {1}  [{2}]",
                              portName, devId, mode);

            if (mode == "arm") Arm();
            else Check();

            Console.WriteLine();
            Console.WriteLine("Link: {0} exchanges, {1} retried, {2} unanswered.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine("{0} passed, {1} failed.", pass, fail);
            port.Close();
            return fail == 0 ? 0 : 1;
        }

        // ---------------------------------------------------------------- arm

        static void Arm()
        {
            Head("Checking it is safe to arm");
            var v = ReadNum(Proto.ID_SFVER);
            Console.WriteLine("    firmware {0}", Show(v));
            if (v == null || v < 106) { Fail("version", "need 1.0.6"); return; }

            if (!Unlock()) return;
            var cal0 = ReadCal();
            Console.WriteLine("    customer correction {0} tenths", Show(cal0));
            if (cal0 != 0)
            { Fail("precondition", "a calibration of " + Show(cal0) + " is already stored"); return; }

            int origUnit = ReadNum(Proto.ID_TMUNIT) ?? -1;
            Console.WriteLine("    unit is {0}", origUnit == 1 ? "Fahrenheit" : "Celsius");
            Pass("precondition", "nothing stored, safe to arm");

            Head("Storing a calibration with the display in Fahrenheit");
            if (origUnit != 1)
            {
                if (!Write(Proto.ID_TMUNIT, Proto.Field5(1), "unit to F")) return;
                System.Threading.Thread.Sleep(800);
            }
            if (ReadNum(Proto.ID_TMUNIT) != 1) { Fail("unit", "could not switch to Fahrenheit"); return; }

            float c0 = LiveC();
            float f0 = c0 * 1.8f + 32.0f;
            int refTenthsF = (int)Math.Round((f0 - 1.8f) * 10f);
            Console.WriteLine("    reads {0} C ({1} F); calibrating against {2} F",
                              F(c0), F(f0),
                              (refTenthsF / 10m).ToString("0.0", CultureInfo.InvariantCulture));

            if (!Unlock()) return;
            if (!Write(Proto.ID_TMCAL, Proto.Field5(refTenthsF), "calibration")) return;
            System.Threading.Thread.Sleep(1300);

            if (!Unlock()) return;
            var cal1 = ReadCal();
            float c1 = LiveC();
            if (cal1 == null || float.IsNaN(c1)) { Fail("arm", "could not read back"); return; }

            Console.WriteLine();
            Console.WriteLine("    correction stored: {0} tenths ({1} C)",
                              cal1, (cal1.Value / 10m).ToString("0.0", CultureInfo.InvariantCulture));
            Console.WriteLine("    device now reads:  {0} C  =  {1} F",
                              F(c1), F(c1 * 1.8f + 32.0f));

            if (cal1 == 0) { Fail("arm", "the correction came out zero, nothing to test"); return; }
            Pass("armed", "a correction of " + cal1 + " tenths is stored, display in Fahrenheit");

            File.WriteAllText(StateFile, string.Join(Environment.NewLine, new string[] {
                "correction=" + cal1,
                "tempC=" + c1.ToString("R", CultureInfo.InvariantCulture),
                "origUnit=" + origUnit,
            }));

            Console.WriteLine();
            Console.WriteLine("    Recorded. Under 1.0.5 the next boot would turn {0} into {1}",
                              cal1, (int)Math.Round(cal1.Value * 1.8 + 32.0));
            Console.WriteLine("    and the reading would move by about {0} C.",
                              F((float)((cal1.Value * 1.8 + 32.0 - cal1.Value) / 10.0)));
            Console.WriteLine();
            Console.WriteLine("    >>> Power-cycle the device now, then run this with 'check'.");
        }

        // ---------------------------------------------------------------- check

        static void Check()
        {
            if (!File.Exists(StateFile))
            { Fail("state", "nothing was armed - run with 'arm' first"); return; }

            int wantCal = 0, origUnit = -1;
            float wantC = float.NaN;
            foreach (var line in File.ReadAllLines(StateFile))
            {
                var bits = line.Split('=');
                if (bits.Length != 2) continue;
                if (bits[0] == "correction") wantCal = int.Parse(bits[1]);
                if (bits[0] == "origUnit") origUnit = int.Parse(bits[1]);
                if (bits[0] == "tempC")
                    wantC = float.Parse(bits[1], CultureInfo.InvariantCulture);
            }

            Head("What was stored before the power cycle");
            Console.WriteLine("    correction {0} tenths, reading {1} C, display in Fahrenheit",
                              wantCal, F(wantC));

            Head("What the device holds now");
            var unit = ReadNum(Proto.ID_TMUNIT);
            Console.WriteLine("    unit is {0}", unit == 1 ? "Fahrenheit" : "Celsius");
            if (unit == 1) Pass("unit", "still Fahrenheit, so the boot path under test ran");
            else Fail("unit", "expected Fahrenheit, reads " + Show(unit));

            if (!Unlock()) return;
            var gotCal = ReadCal();
            float gotC = LiveC();
            Console.WriteLine("    correction {0} tenths ({1} C)", Show(gotCal),
                              gotCal == null ? "?" : (gotCal.Value / 10m).ToString("0.0", CultureInfo.InvariantCulture));
            Console.WriteLine("    device reads {0} C  =  {1} F", F(gotC), F(gotC * 1.8f + 32.0f));

            int wouldBe = (int)Math.Round(wantCal * 1.8 + 32.0);
            Console.WriteLine();
            if (gotCal == wantCal)
                Pass("correction persisted", wantCal + " tenths, unchanged by the power cycle");
            else if (gotCal == wouldBe)
                Fail("correction persisted", "it became " + gotCal + ", which is "
                     + wantCal + "*1.8+32 - the boot conversion is still there");
            else
                Fail("correction persisted", "expected " + wantCal + ", got " + Show(gotCal));

            if (!float.IsNaN(gotC) && !float.IsNaN(wantC))
            {
                float moved = gotC - wantC;
                Console.WriteLine("    the reading moved {0} C across the power cycle", F(moved));
                if (Math.Abs(moved) < 0.5f)
                    Pass("reading persisted", "within normal sensor drift");
                else
                    Fail("reading persisted", "moved " + F(moved) + " C, far more than drift");
            }

            // ---- restore
            Head("Restoring");
            if (Unlock())
            {
                Write(Proto.ID_DFLT_CAL, "1", "clear calibration");
                System.Threading.Thread.Sleep(900);
                if (Unlock())
                {
                    var back = ReadCal();
                    Console.WriteLine("    correction is now {0} tenths", Show(back));
                    if (back == 0) Pass("restore calibration", "cleared");
                    else Fail("restore calibration", "left at " + Show(back));
                }
            }
            if (origUnit == 0 || origUnit == 1)
            {
                Write(Proto.ID_TMUNIT, Proto.Field5(origUnit), "unit");
                System.Threading.Thread.Sleep(800);
                var u = ReadNum(Proto.ID_TMUNIT);
                Console.WriteLine("    unit is back to {0}", u == 1 ? "Fahrenheit" : "Celsius");
                if (u == origUnit) Pass("restore unit", "as it was");
                else Fail("restore unit", "reads " + Show(u));
            }
            try { File.Delete(StateFile); } catch { }
        }

        // ---------------------------------------------------------------- helpers

        static bool Unlock()
        {
            var r = link.Exchange(Proto.BuildWrite(devId, Proto.ID_CAL_CPWD, Proto.Field5(pwd)),
                                  0, "unlock");
            if (r != null && r.CrcOk && !r.InvalidPara) return true;
            Fail("unlock", "password not accepted");
            return false;
        }

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
