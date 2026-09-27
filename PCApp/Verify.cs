// End-to-end verification against the real device, driven through the application's
// own Link class - the same pacing and retry code the shipping .exe uses.
//
// This is the run that has to come out clean: the application's "Read all" issues this
// same burst of back-to-back reads, which is exactly what the unpaced harness lost
// frames on.
//
// READ ONLY.
//
// Build:  csc /target:exe /main:NiyamaConfig.Verify /out:Verify.exe NiyamaConfig.cs Verify.cs

using System;
using System.Globalization;
using System.IO.Ports;
using System.Linq;

namespace NiyamaConfig
{
    public static class Verify
    {
        static Link link;
        static byte devId = 1;

        public static int Main(string[] argv)
        {
            string portName = argv.Length > 0 ? argv[0] : "COM3";
            if (argv.Length > 1) devId = byte.Parse(argv[1]);

            var port = new SerialPort(portName, 57600, Parity.None, 8, StopBits.One);
            port.ReadTimeout = 500; port.WriteTimeout = 500;
            port.Open();
            link = new Link(port, delegate(string s) { Console.WriteLine("      . " + s); },
                            delegate { return false; });

            Console.WriteLine("Driving the application's own Link against {0}, device {1}.",
                              portName, devId);
            Console.WriteLine("Read only - no write command is sent.");
            Console.WriteLine();

            int bad = 0;
            bad += Sweep();
            bad += Live();
            bad += Indexed();

            Console.WriteLine();
            Console.WriteLine("Link totals: {0} exchanges, {1} needed a retry, {2} gave up.",
                              link.Sent, link.Retried, link.Lost);
            Console.WriteLine(bad == 0 && link.Lost == 0
                ? "CLEAN - every request was answered."
                : bad + " problem(s); " + link.Lost + " request(s) unanswered.");

            port.Close();
            return (bad == 0 && link.Lost == 0) ? 0 : 1;
        }

        static int Sweep()
        {
            Console.WriteLine("=== Every parameter in the application's table, back to back");
            Console.WriteLine("  {0,-11} {1,-27} {2,-5} {3,10} {4,12}  {5}",
                              "group", "parameter", "id", "raw", "scaled", "units");
            Console.WriteLine("  " + new string('-', 80));

            int ok = 0, na = 0, dead = 0;
            string lastGroup = null;
            foreach (var pm in Params.All)
            {
                if (lastGroup != null && pm.Group != lastGroup) Console.WriteLine();
                lastGroup = pm.Group;

                var r = link.Exchange(Proto.BuildRead(devId, pm.Id, null), 0, pm.Name);
                string raw, scaled;
                if (r == null || !r.CrcOk) { raw = "-"; scaled = "NO REPLY"; dead++; }
                else if (r.InvalidPara) { raw = "-"; scaled = "not in build"; na++; }
                else
                {
                    raw = r.Text;
                    double v;
                    scaled = double.TryParse(r.Text, NumberStyles.Integer,
                                             CultureInfo.InvariantCulture, out v)
                        ? (v / pm.Scale).ToString("F" + pm.Decimals, CultureInfo.InvariantCulture)
                        : "(not a number)";
                    ok++;
                }
                Console.WriteLine("  {0,-11} {1,-27} 0x{2:X2}  {3,10} {4,12}  {5}",
                                  pm.Group, pm.Name, pm.Id, raw, scaled, pm.Units);
            }
            Console.WriteLine();
            Console.WriteLine("  {0} answered, {1} not present in this build, {2} unanswered.",
                              ok, na, dead);
            return dead;
        }

        static int Live()
        {
            Console.WriteLine();
            Console.WriteLine("=== Real-time frame, 12 reads one second apart (the rate the app polls at)");
            int bad = 0;
            float[] first = null;
            for (int i = 0; i < 12; i++)
            {
                if (i > 0) System.Threading.Thread.Sleep(1000);
                var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                      Proto.REALTIME_LEN, "live");
                if (r == null || !r.CrcOk || r.Payload.Length < 44)
                { Console.WriteLine("  read {0}: FAILED", i); bad++; continue; }

                var p = r.Payload;
                float[] v = { BitConverter.ToSingle(p, 5), BitConverter.ToSingle(p, 9),
                              BitConverter.ToSingle(p, 13) };
                if (first == null) first = v;

                Console.WriteLine("  read {0,2}: len {1}  crc ok  epoch {2}   {3,8:0.000} {4,8:0.000} {5,8:0.000}"
                                  + (r.Raw.Take(r.Raw.Length - 1).Contains((byte)0xFC)
                                     ? "   <- contains 0xFC in its data" : ""),
                                  i, r.Raw.Length, BitConverter.ToUInt32(p, 0), v[0], v[1], v[2]);
            }
            Console.WriteLine(bad == 0 ? "  every frame arrived intact." : "  " + bad + " failed.");
            return bad;
        }

        static int Indexed()
        {
            Console.WriteLine();
            Console.WriteLine("=== Indexed parameters");
            int bad = 0;

            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_DP_LIMIT, "0"), 0, "DP1 clamp");
            int v;
            if (r != null && r.CrcOk && !r.InvalidPara && int.TryParse(r.Text, out v))
                Console.WriteLine("  DP1 clamp 0x6E = {0} -> {1} Pa",
                                  r.Text, (v / 10.0).ToString("0.0", CultureInfo.InvariantCulture));
            else { Console.WriteLine("  DP1 clamp: FAILED"); bad++; }

            Console.Write("  DP1 slot offsets 0x72:");
            for (int slot = 0; slot < 6; slot++)
            {
                var s = link.Exchange(Proto.BuildRead(devId, Proto.ID_DP_SLOT_OFFSET,
                                                      "0" + slot), 0, "slot " + slot);
                if (s != null && s.CrcOk && !s.InvalidPara && int.TryParse(s.Text, out v))
                    Console.Write("  [{0}]={1}", slot,
                                  (v / 10.0).ToString("0.0", CultureInfo.InvariantCulture));
                else { Console.Write("  [{0}]=FAIL", slot); bad++; }
            }
            Console.WriteLine();

            var z = link.Exchange(Proto.BuildRead(devId, Proto.ID_DP_OFFSET, "0"), 0, "DP1 zero");
            if (z != null && z.CrcOk && !z.InvalidPara && int.TryParse(z.Text, out v))
                Console.WriteLine("  DP1 zero offset 0x71 = {0} -> {1} Pa",
                                  z.Text, (v / 100.0).ToString("0.00", CultureInfo.InvariantCulture));
            else { Console.WriteLine("  DP1 zero offset: FAILED"); bad++; }

            return bad;
        }
    }
}
