// Why does 0x20 (temperature minimum) not answer while 0x21 does?
//
// Reads both repeatedly, and reads the same value out of the live frame, which takes a
// completely different path through the firmware. If 0x20 stays silent while the frame
// reports a perfectly good minimum, the fault is in that one read handler.
//
// READ ONLY.
//
// Build:  csc /target:exe /main:NiyamaConfig.MinProbe /out:MinProbe.exe
//              NiyamaConfig.cs MinProbe.cs

using System;
using System.Globalization;
using System.IO.Ports;

namespace NiyamaConfig
{
    public static class MinProbe
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
            link = new Link(port, delegate { }, delegate { return false; });

            Console.WriteLine("Probing the temperature extremes on {0}.", portName);
            Console.WriteLine();

            // the live frame carries min and max directly, bypassing the read dispatch
            var r = link.Exchange(Proto.BuildRead(devId, Proto.ID_REALTIME, null),
                                  Proto.REALTIME_LEN, "live");
            if (r != null && r.CrcOk && r.Payload.Length >= 44)
            {
                Console.WriteLine("From the live frame (a different code path):");
                Console.WriteLine("   temperature {0,8:0.00} C",
                                  BitConverter.ToSingle(r.Payload, 9));
                Console.WriteLine("   minimum     {0,8:0.00} C",
                                  BitConverter.ToSingle(r.Payload, 25));
                Console.WriteLine("   maximum     {0,8:0.00} C",
                                  BitConverter.ToSingle(r.Payload, 29));
            }

            Console.WriteLine();
            Console.WriteLine("Ten reads of each parameter:");
            Console.WriteLine("   {0,-6} {1,-26} {2}", "id", "what", "results");
            Probe(0x20, "Temp minimum");
            Probe(0x21, "Temp maximum");
            Probe(0x22, "RH minimum");
            Probe(0x1C, "DP1 minimum");

            port.Close();
            return 0;
        }

        static void Probe(byte id, string name)
        {
            int ok = 0, silent = 0, invalid = 0;
            string sample = "";
            for (int i = 0; i < 10; i++)
            {
                var r = link.Exchange(Proto.BuildRead(devId, id, null), 0, name);
                if (r == null || !r.CrcOk) silent++;
                else if (r.InvalidPara) invalid++;
                else { ok++; if (sample.Length == 0) sample = r.Text; }
            }
            Console.WriteLine("   0x{0:X2}   {1,-26} {2} answered, {3} silent, {4} invalid{5}",
                              id, name, ok, silent, invalid,
                              sample.Length > 0 ? "   e.g. \"" + sample + "\"" : "");
        }
    }
}
