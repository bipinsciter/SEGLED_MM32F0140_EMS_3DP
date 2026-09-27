// Hardware test for the newly added features.
//
// Covers: the serial number (read, write, restore), the DP zero offset in Pa, and the
// temperature / humidity calibration READS both locked and unlocked.
//
// It deliberately does NOT perform a calibration write.  That stores a correction
// derived from whatever the sensor happens to be reading and resets the recorded
// minimum and maximum, which changes the instrument's accuracy - not something to do
// unasked.
//
// Build:  csc /target:exe /main:NiyamaConfig.NewFeat /out:NewFeat.exe
//              NiyamaConfig.cs NewFeat.cs

using System;
using System.Globalization;
using System.IO.Ports;
using System.Linq;
using System.Text;

namespace NiyamaConfig
{
    public static class NewFeat
    {
        static Link link;
        static byte devId = 1;
        static int pass, fail;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            int pwd = argv.Length > 2 ? int.Parse(argv[2]) : 100;

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("New-feature test on {0}, device {1}.", portName, devId);
            Console.WriteLine("No calibration write is performed.");

            RetiredIds();
            Serial();
            ZeroOffset();
            CalibrationReads(pwd);

            Console.WriteLine();
            Console.WriteLine("Link totals: {0} exchanges, {1} retried, {2} unanswered.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine("{0} passed, {1} failed.", pass, fail);
            port.Close();
            return fail == 0 ? 0 : 1;
        }

        static void RetiredIds()
        {
            Head("0x55 and 0x56 are gone from the application's table");
            bool present = Params.All.Any(p => p.Id == 0x55 || p.Id == 0x56);
            if (present) Fail("retired ids", "still listed in the parameter table");
            else Pass("retired ids", "neither appears in the parameter table");

            // The firmware only ever declared them, so the device answers INVALID_PARA
            // whether or not the defines are there.
            foreach (byte id in new byte[] { 0x55, 0x56 })
            {
                var r = link.Exchange(Proto.BuildRead(devId, id, null), 0, "0x" + id.ToString("X2"));
                Console.WriteLine("    device on 0x{0:X2}: {1}", id,
                    r == null ? "no reply" : !r.CrcOk ? "bad crc"
                    : r.InvalidPara ? "INVALID_PARA, as expected" : "answered \"" + r.Text + "\"");
            }
        }

        static void Serial()
        {
            Head("Serial number (0x40)");

            string orig = ReadSerial();
            if (orig == null) { Fail("serial read", "no usable reply"); return; }
            Console.WriteLine("    stored : \"{0}\"", orig);
            Pass("serial read", "16 characters returned");

            const string test = "NIYTEST0000000Z9";
            Console.WriteLine("    writing: \"{0}\"", test);
            var w = link.Exchange(Proto.BuildWrite(devId, Proto.ID_SRNO, test), 0, "write serial");
            if (w == null || !w.CrcOk) { Fail("serial write", "no reply"); return; }

            string back = ReadSerial();
            Console.WriteLine("    reads  : \"{0}\"", back);
            if (back != test) { Fail("serial write", "read back as \"" + back + "\""); }
            else Pass("serial write", "written and verified");

            // put the original back whatever happened above
            link.Exchange(Proto.BuildWrite(devId, Proto.ID_SRNO, orig), 0, "restore serial");
            string done = ReadSerial();
            Console.WriteLine("    restored to \"{0}\"", done);
            if (done == orig) Pass("serial restore", "back to the original");
            else Fail("serial restore", "left as \"" + done + "\"");
        }

        static string ReadSerial()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_SRNO, null),
                                  Proto.SRNO_LEN, "read serial");
            if (r == null || !r.CrcOk || r.Payload.Length < Proto.SRNO_CHARS) return null;
            return Encoding.ASCII.GetString(r.Payload, 0, Proto.SRNO_CHARS);
        }

        static void ZeroOffset()
        {
            Head("DP zero offset entered in Pa (0x71)");
            Console.WriteLine("    The box now holds Pa to two decimals; the wire still carries");
            Console.WriteLine("    hundredths, so 1.25 Pa has to go out as 00125.");

            decimal pa = 1.25m;
            int wire = (int)Math.Round(pa * 100m);
            Console.WriteLine("    {0} Pa -> wire {1} -> field \"{2}\"",
                              pa, wire, Proto.SignedField5(wire));
            if (wire == 125 && Proto.SignedField5(wire) == "+00125")
                Pass("offset encoding", "1.25 Pa becomes +00125");
            else Fail("offset encoding", "got " + Proto.SignedField5(wire));

            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_DP_OFFSET, "0"), 0, "zero offset");
            int v;
            if (r != null && r.CrcOk && !r.InvalidPara
                && int.TryParse(r.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out v))
            {
                Console.WriteLine("    device DP1 zero offset: raw {0} = {1} Pa",
                                  v, (v / 100m).ToString("0.00", CultureInfo.InvariantCulture));
                Pass("offset read", "scales to Pa correctly");
            }
            else Fail("offset read", "could not read it");
        }

        static void CalibrationReads(int pwd)
        {
            Head("Temperature and humidity calibration reads (0x32 / 0x33)");

            Console.WriteLine("  -- locked (expecting a 12 byte reply once the firmware fix is on)");
            ShowCal(0x32, "temperature", Proto.CAL_LEN_LOCKED);
            ShowCal(0x33, "humidity", Proto.CAL_LEN_LOCKED);

            Console.WriteLine();
            Console.WriteLine("  -- unlocking (customer, password {0})", pwd);
            var u = link.Exchange(Proto.BuildWrite(devId, Proto.ID_CAL_CPWD, Proto.Field5(pwd)),
                                  0, "unlock");
            Console.WriteLine("     {0}", u == null ? "no reply"
                                        : u.CrcOk ? "accepted (" + Proto.StatusText(u.Status) + ")"
                                        : "bad crc");

            Console.WriteLine();
            Console.WriteLine("  -- unlocked (expecting {0} bytes)", Proto.CAL_LEN_CUSTOMER);
            bool a = ShowCal(0x32, "temperature", Proto.CAL_LEN_CUSTOMER);
            bool b = ShowCal(0x33, "humidity", Proto.CAL_LEN_CUSTOMER);
            if (a && b) Pass("calibration reads", "both answered at the expected length");
            else Fail("calibration reads", "one or both did not answer as expected");
        }

        static bool ShowCal(byte id, string name, int expect)
        {
            var r = link.Exchange(Proto.BuildRead(devId, id, null), expect, name);
            if (r == null || !r.CrcOk)
            {
                Console.WriteLine("     {0,-12} 0x{1:X2}  no usable reply at {2} bytes",
                                  name, id, expect);
                return false;
            }
            int v;
            bool ok = r.Text.Length >= 5
                      && int.TryParse(r.Text.Substring(0, 5), NumberStyles.Integer,
                                      CultureInfo.InvariantCulture, out v);
            v = ok ? int.Parse(r.Text.Substring(0, 5), CultureInfo.InvariantCulture) : 0;
            Console.WriteLine("     {0,-12} 0x{1:X2}  {2} bytes, correction {3}",
                              name, id, r.Raw.Length,
                              ok ? (v / 10.0).ToString("0.0", CultureInfo.InvariantCulture)
                                 : "unreadable");
            return ok;
        }

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
