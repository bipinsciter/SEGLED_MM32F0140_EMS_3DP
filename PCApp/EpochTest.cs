// Check the device's own epoch against its own clock.
//
// The device exposes both: 0x4B returns the RTC fields as ASCII, and the live frame
// carries the epoch that get_epoch_time() derived from those same fields. Reading
// both and converting independently tests the firmware's conversion on real hardware,
// against real dates, with no reliance on my reading of the algorithm.
//
// READ ONLY.
//
// Build:  csc /target:exe /main:NiyamaConfig.EpochTest /out:EpochTest.exe
//              NiyamaConfig.cs EpochTest.cs

using System;
using System.Globalization;
using System.IO.Ports;
using System.Text;

namespace NiyamaConfig
{
    public static class EpochTest
    {
        static Link link;
        static byte devId = 1;
        static int pass, fail;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);
            int rounds = argv.Length > 2 ? int.Parse(argv[2]) : 5;

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("        . " + s); },
                            delegate { return false; });

            Console.WriteLine("Epoch conversion check on {0}, device {1}.", portName, devId);
            Console.WriteLine("Comparing the device's epoch against its own RTC fields.");
            Console.WriteLine();
            Console.WriteLine("{0,-21} {1,12} {2,12} {3,7}  {4}",
                              "device clock", "its epoch", "expected", "diff", "verdict");

            for (int i = 0; i < rounds; i++)
            {
                Round();
                if (i < rounds - 1) System.Threading.Thread.Sleep(1500);
            }

            Console.WriteLine();
            Console.WriteLine("Link: {0} exchanges, {1} retried, {2} unanswered.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine("{0} passed, {1} failed.", pass, fail);
            port.Close();
            return fail == 0 ? 0 : 1;
        }

        static void Round()
        {
            // Clock first, then the epoch, and the clock again: if the second reading
            // differs the RTC ticked mid-test and the comparison has to allow for it.
            DateTime? before = ReadClock();
            uint? epoch = ReadEpoch();
            DateTime? after = ReadClock();

            if (before == null || after == null || epoch == null)
            { Fail("no reply", "one of the reads did not come back"); return; }

            long wantBefore = ToEpoch(before.Value);
            long wantAfter = ToEpoch(after.Value);
            long got = epoch.Value;

            // Accept anything between the two clock readings, plus a second of slack
            // for the ordering of the three exchanges.
            long lo = Math.Min(wantBefore, wantAfter) - 1;
            long hi = Math.Max(wantBefore, wantAfter) + 1;
            long diff = got - wantBefore;

            bool ok = got >= lo && got <= hi;
            Console.WriteLine("{0,-21} {1,12} {2,12} {3,7}  {4}",
                              before.Value.ToString("dd-MMM-yyyy HH:mm:ss"),
                              got, wantBefore, diff,
                              ok ? "ok" : "MISMATCH");
            if (ok) pass++;
            else
            {
                fail++;
                var asDate = new DateTime(1970, 1, 1).AddSeconds(got);
                Console.WriteLine("        the device's epoch decodes to {0}",
                                  asDate.ToString("dd-MMM-yyyy HH:mm:ss"));
            }
        }

        /// Seconds since 1970-01-01, treating the RTC as the basis - which is what the
        /// firmware does, since EPOCH_YEAR is 1970 and it applies no timezone.
        static long ToEpoch(DateTime t)
        {
            return (long)(t - new DateTime(1970, 1, 1)).TotalSeconds;
        }

        static DateTime? ReadClock()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_DATETIME, null),
                                  Proto.DATETIME_LEN, "clock");
            if (r == null || !r.CrcOk || r.Payload.Length < 12) return null;

            string s = Encoding.ASCII.GetString(r.Payload, 0, 12);   // DDMMYYhhmmss
            int dd, mo, yy, hh, mi, ss;
            if (!int.TryParse(s.Substring(0, 2), out dd) || !int.TryParse(s.Substring(2, 2), out mo)
             || !int.TryParse(s.Substring(4, 2), out yy) || !int.TryParse(s.Substring(6, 2), out hh)
             || !int.TryParse(s.Substring(8, 2), out mi) || !int.TryParse(s.Substring(10, 2), out ss))
                return null;
            try { return new DateTime(2000 + yy, mo, dd, hh, mi, ss); }
            catch (ArgumentOutOfRangeException) { return null; }
        }

        static uint? ReadEpoch()
        {
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                  Proto.REALTIME_LEN, "live");
            if (r == null || !r.CrcOk || r.Payload.Length < 44) return null;
            return BitConverter.ToUInt32(r.Payload, 0);
        }

        static void Fail(string w, string d) { fail++; Console.WriteLine("    FAIL  " + w + " - " + d); }
    }
}
