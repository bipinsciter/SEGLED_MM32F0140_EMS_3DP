// Protocol self-test for the NIYAMA_3DP configuration tool.
//
// It links against the real NiyamaConfig.cs, so the code under test is the code the
// application ships.  The firmware side - CalCRC() and findValue() from main.c - is
// reimplemented here so each frame the application builds is decoded the way the
// device would decode it.
//
// Build:  csc /target:exe /main:NiyamaConfig.SelfTest /out:SelfTest.exe
//              NiyamaConfig.cs SelfTest.cs

using System;
using System.Linq;
using System.Text;

namespace NiyamaConfig
{
    static class Fw    // transcribed from main.c
    {
        public static byte CalCRC(byte[] buf, int offset, int count)
        {
            uint total = 0x55;
            for (int i = 0; i < count; i++) total += buf[offset + i];
            if (total > 0x7F) total &= 0x7F;
            return (byte)total;
        }

        /// findValue(uint8_t *ptr, uint8_t NoOfDigit) - reads the field back to front and
        /// treats a leading '-' as consuming one of the NoOfDigit characters.
        public static short findValue(byte[] buf, int ptr, int noOfDigit)
        {
            ushort value = 0, value1 = 1;
            bool minus = false;
            if (buf[ptr] == (byte)'-') { ptr += noOfDigit - 1; noOfDigit--; minus = true; }
            else ptr += noOfDigit - 1;

            while (noOfDigit > 0)
            {
                value += (ushort)((buf[ptr] - '0') * value1);
                ptr--; noOfDigit--; value1 *= 10;
            }
            return minus ? (short)(-value) : (short)value;
        }
    }

    public static class SelfTest
    {
        static int failed, run;

        static void Check(string what, bool ok, string detail)
        {
            run++;
            if (!ok) { failed++; Console.WriteLine("  FAIL  " + what + "   " + detail); }
            else Console.WriteLine("  ok    " + what + (detail.Length > 0 ? "   " + detail : ""));
        }

        static void Eq(string what, object got, object want)
        {
            Check(what, Equals(got.ToString(), want.ToString()),
                  "got " + got + ", expected " + want);
        }

        public static int Main()
        {
            Console.WriteLine("CRC");
            // The firmware CRCs bytes 1..n-2 of the frame; Proto.Crc CRCs the body it is
            // about to wrap.  The two must agree on the same bytes.
            var f = Proto.BuildRead(1, 0x34, null);
            Eq("read frame length", f.Length, 6);
            Eq("start byte", f[0].ToString("X2"), "FF");
            Eq("end byte", f[5].ToString("X2"), "FE");
            Eq("crc matches the firmware's", f[4], Fw.CalCRC(f, 1, 3));
            Check("crc never exceeds 0x7F", f[4] <= 0x7F, "crc=" + f[4].ToString("X2"));

            // A body that overflows 0x7F exercises the masking branch.
            var big = Proto.BuildWrite(250, 0x6E, "299999");
            Eq("crc masks when it overflows", big[big.Length - 2],
               Fw.CalCRC(big, 1, big.Length - 3));

            Console.WriteLine();
            Console.WriteLine("Value fields, decoded by the firmware's findValue");
            // Alarm setpoint: the grid holds 55.0 Pa, scale 100 -> 5500 on the wire, and
            // the firmware divides by 10 to reach the 550 tenths it stores.
            var p1 = Bytes(Proto.Field5(5500));
            Eq("55.0 Pa encodes as", Proto.Field5(5500), "05500");
            Eq("firmware recovers", Fw.findValue(p1, 0, 5), 5500);
            Eq("firmware stores tenths", Fw.findValue(p1, 0, 5) / 10, 550);

            // DP limit 300.0 Pa -> 3000 tenths.
            Eq("DP limit 3000", Proto.Field5(3000), "03000");
            Eq("firmware recovers", Fw.findValue(Bytes(Proto.Field5(3000)), 0, 5), 3000);

            // Slot offsets, including negatives, where the sign lives inside the field.
            foreach (int v in new[] { 0, 7, -7, 15, -15, 999, -999, 5000, -4999 })
            {
                var s = Proto.Field5(v);
                Check("slot offset " + v + " -> \"" + s + "\"",
                      s.Length == 5 && Fw.findValue(Bytes(s), 0, 5) == v,
                      "firmware read back " + Fw.findValue(Bytes(s), 0, 5));
            }

            // The zero offset keeps its sign in a byte of its own ahead of five digits.
            foreach (int v in new[] { 0, 250, -250, 9999 })
            {
                var s = Proto.SignedField5(v);
                bool neg = s[0] == '-';
                int got = Fw.findValue(Bytes(s), 1, 5) * (neg ? -1 : 1);
                Check("zero offset " + v + " -> \"" + s + "\"",
                      s.Length == 6 && got == v, "firmware read back " + got);
            }

            // findValue() returns an int16_t, so anything past 32767 wraps on the device.
            // The application has to refuse those rather than send a value that will be
            // stored as something else entirely.
            Check("32767 is accepted", Proto.FitsOnWire(32767), "");
            Check("-32768 is accepted", Proto.FitsOnWire(-32768), "");
            Check("32768 is refused", !Proto.FitsOnWire(32768), "");
            Check("99999 is refused", !Proto.FitsOnWire(99999), "");
            Check("  and it really would have wrapped",
                  (int)Fw.findValue(Bytes("99999"), 0, 5) != 99999,
                  "findValue gave " + Fw.findValue(Bytes("99999"), 0, 5));

            Console.WriteLine();
            Console.WriteLine("Indexed frame layout");
            // DP_SLOT_OFFSET_ID: RxBuffer[4]=channel, [5]=slot, [6..10]=value.
            var slotFrame = Proto.BuildWrite(1, 0x72, "2" + "4" + Proto.Field5(-33));
            Eq("channel byte at RxBuffer[4]", (char)slotFrame[4], '2');
            Eq("slot byte at RxBuffer[5]", (char)slotFrame[5], '4');
            Eq("value at RxBuffer[6], 5 wide", Fw.findValue(slotFrame, 6, 5), -33);

            // DP_LIMIT_ID: RxBuffer[4]=channel, [5..9]=value.
            var limFrame = Proto.BuildWrite(1, 0x6E, "0" + Proto.Field5(3000));
            Eq("limit channel at RxBuffer[4]", (char)limFrame[4], '0');
            Eq("limit value at RxBuffer[5]", Fw.findValue(limFrame, 5, 5), 3000);

            // DATETIME_ID: RxBuffer[4..15] = DDMMYYHHMMSS.
            var dt = new DateTime(2026, 9, 26, 14, 5, 9);
            var dtFrame = Proto.BuildWrite(1, 0x4B,
                              dt.ToString("ddMMyyHHmmss", System.Globalization.CultureInfo.InvariantCulture));
            Eq("datetime payload", Encoding.ASCII.GetString(dtFrame, 4, 12), "260926140509");
            Eq("  day  from RxBuffer[4]", Two(dtFrame, 4), 26);
            Eq("  month from RxBuffer[6]", Two(dtFrame, 6), 9);
            Eq("  year  from RxBuffer[8]", 2000 + Two(dtFrame, 8), 2026);
            Eq("  hour  from RxBuffer[10]", Two(dtFrame, 10), 14);
            Eq("  min   from RxBuffer[12]", Two(dtFrame, 12), 5);
            Eq("  sec   from RxBuffer[14]", Two(dtFrame, 14), 9);

            Console.WriteLine();
            Console.WriteLine("Response parsing");
            // An ordinary ASCII read reply.
            var ascii = FwResponse(1, 0x10, 0x00, 0x34, Bytes("103"));
            var rx = ascii.ToList();
            var r1 = Proto.TryParse(rx, 0);
            Check("ascii reply parses", r1 != null, "");
            Eq("  crc accepted", r1.CrcOk, true);
            Eq("  parameter id", r1.ParamId, 0x34);
            Eq("  text", r1.Text, "103");
            Eq("  buffer drained", rx.Count, 0);

            // A short reply must not be consumed until it is complete.
            var partial = ascii.Take(4).ToList();
            Check("incomplete reply is held back", Proto.TryParse(partial, 0) == null,
                  "returned a frame too early");

            // The real-time reply is binary and fixed length.  This one carries a float
            // whose bytes contain 0xFC, which is exactly what would break a scan for the
            // terminator.
            float sneaky = BitConverter.ToSingle(new byte[] { 0x00, 0x00, 0xFC, 0x42 }, 0); // 126.0
            var rt = RealtimeFrame(1, 0x00, 1758888000u, 0x00,
                                   sneaky, 23.5f, 48.2f,
                                   -1.5f, 12.75f, 20.0f, 27.0f, 40.0f, 55.0f,
                                   1, 0, 2);
            Eq("realtime frame length", rt.Length, Proto.REALTIME_LEN);
            Check("frame really does contain a 0xFC in its data",
                  rt.Take(rt.Length - 1).Contains((byte)0xFC), "");

            var rxr = rt.ToList();
            var r2 = Proto.TryParse(rxr, Proto.REALTIME_LEN);
            Check("realtime reply parses", r2 != null, "");
            Eq("  crc accepted", r2.CrcOk, true);
            Eq("  payload length", r2.Payload.Length, 44);
            var p = r2.Payload;
            Eq("  epoch", BitConverter.ToUInt32(p, 0), 1758888000u);
            Eq("  epoch as a date",
               new DateTime(1970, 1, 1).AddSeconds(BitConverter.ToUInt32(p, 0)).ToString("dd-MMM-yyyy HH:mm:ss"),
               new DateTime(1970, 1, 1).AddSeconds(1758888000).ToString("dd-MMM-yyyy HH:mm:ss"));
            Eq("  channel 1 value (the 0xFC one)", BitConverter.ToSingle(p, 5), sneaky);
            Eq("  channel 2 value", BitConverter.ToSingle(p, 9), 23.5f);
            Eq("  channel 3 value", BitConverter.ToSingle(p, 13), 48.2f);
            Eq("  channel 1 min", BitConverter.ToSingle(p, 17), -1.5f);
            Eq("  channel 1 max", BitConverter.ToSingle(p, 21), 12.75f);
            Eq("  channel 2 min", BitConverter.ToSingle(p, 25), 20.0f);
            Eq("  channel 2 max", BitConverter.ToSingle(p, 29), 27.0f);
            Eq("  channel 3 min", BitConverter.ToSingle(p, 33), 40.0f);
            Eq("  channel 3 max", BitConverter.ToSingle(p, 37), 55.0f);
            Eq("  channel 1 alarm", p[41], 1);
            Eq("  channel 2 alarm", p[42], 0);
            Eq("  channel 3 alarm", p[43], 2);
            Eq("  buffer drained", rxr.Count, 0);

            // Leading noise before the start byte must be discarded, not choked on.
            var noisy = new byte[] { 0x00, 0xAB, 0xFF }.Concat(ascii).ToList();
            var r3 = Proto.TryParse(noisy, 0);
            Check("leading noise is skipped", r3 != null && r3.CrcOk && r3.Text == "103", "");

            // A corrupted CRC must be reported, not silently accepted.
            var bad = (byte[])ascii.Clone();
            bad[bad.Length - 2] ^= 0x01;
            var rxb = bad.ToList();
            var r4 = Proto.TryParse(rxb, 0);
            Check("a bad crc is rejected", r4 == null || !r4.CrcOk,
                  r4 == null ? "no frame returned" : "CrcOk was true");

            Console.WriteLine();
            Console.WriteLine("Serial number and calibration framing");

            // The write puts the 16 characters straight at RxBuffer[4]; the firmware
            // memcpy's them from there without any length field.
            var srFrame = Proto.BuildWrite(1, 0x40, "NIY2026XY0000123");
            Eq("serial write frame length", srFrame.Length, 4 + 16 + 2);
            Eq("serial characters land at RxBuffer[4]",
               Encoding.ASCII.GetString(srFrame, 4, 16), "NIY2026XY0000123");
            Eq("serial write crc", srFrame[srFrame.Length - 2],
               Fw.CalCRC(srFrame, 1, srFrame.Length - 3));

            // The reply is a fixed 23 bytes, and those 16 characters are free-form, so
            // it has to be read by length rather than by hunting for the terminator.
            var srReply = FwResponse(1, 0x10, 0x00, 0x40, Bytes("NIY2026XY0000123"));
            Eq("serial reply length", srReply.Length, Proto.SRNO_LEN);
            var srRx = srReply.ToList();
            var sr = Proto.TryParse(srRx, Proto.SRNO_LEN);
            Check("serial reply parses", sr != null && sr.CrcOk, "");
            Eq("  characters recovered",
               Encoding.ASCII.GetString(sr.Payload, 0, Proto.SRNO_CHARS), "NIY2026XY0000123");

            // A serial number containing the terminator byte would defeat a scan; by
            // length it comes back intact.
            // built byte by byte: a C# "\\xFC" literal is the character u-umlaut,
            // which ASCII encoding would turn into a question mark, not the byte 0xFC
            var odd = Bytes("AB0DEFGHIJKLMNO0");
            odd[2] = 0xFC;
            var oddReply = FwResponse(1, 0x10, 0x00, 0x40, odd);
            var oddRx = oddReply.ToList();
            var od = Proto.TryParse(oddRx, Proto.SRNO_LEN);
            Check("a serial containing 0xFC still parses", od != null && od.CrcOk, "");
            Eq("  byte 2 preserved", od.Payload[2], 0xFC);

            // Calibration: the value travels as five characters in tenths, and the
            // firmware reads it with findValue(&RxBuffer[4], 5).
            Eq("25.0 degrees encodes as", Proto.Field5(250), "00250");
            var calFrame = Proto.BuildWrite(1, 0x32, Proto.Field5(250));
            Eq("calibration value at RxBuffer[4]", Fw.findValue(calFrame, 4, 5), 250);
            Eq("-12.5 degrees encodes as", Proto.Field5(-125), "-0125");
            var negFrame = Proto.BuildWrite(1, 0x32, Proto.Field5(-125));
            Eq("negative reference recovered", Fw.findValue(negFrame, 4, 5), -125);

            // The three reply shapes: locked carries the correction alone, unlocked
            // carries the calibration date history behind it.
            foreach (int len in new[] { Proto.CAL_LEN_LOCKED, Proto.CAL_LEN_FACTORY,
                                        Proto.CAL_LEN_CUSTOMER })
            {
                var body = new byte[len - 7];
                var digits = Bytes("00035");                 // 3.5 in tenths, zero padded
                Array.Copy(digits, body, 5);
                for (int i = 5; i < body.Length; i++) body[i] = (byte)(i * 7);   // history
                var rep = FwResponse(1, 0x10, 0x00, 0x32, body);
                var rl = rep.ToList();
                var cr = Proto.TryParse(rl, len);
                Check("calibration reply of " + len + " bytes parses",
                      cr != null && cr.CrcOk, "");
                Eq("  correction from the first 5 characters",
                   int.Parse(cr.Text.Substring(0, 5)), 35);
            }

            Console.WriteLine();
            Console.WriteLine("Status decoding");
            Eq("clean status", Proto.StatusText(0x00), "OK");
            Eq("invalid parameter", Proto.StatusText(0x02), "invalid parameter");
            Eq("rtc invalid", Proto.StatusText(0x40), "RTC invalid");
            Eq("two faults at once", Proto.StatusText(0x04 | 0x40), "DP1 fault, RTC invalid");

            Console.WriteLine();
            Console.WriteLine(failed == 0
                ? string.Format("All {0} checks passed.", run)
                : string.Format("{0} of {1} checks FAILED.", failed, run));
            return failed == 0 ? 0 : 1;
        }

        static byte[] Bytes(string s) { return Encoding.ASCII.GetBytes(s); }

        static int Two(byte[] b, int i) { return (b[i] - '0') * 10 + (b[i + 1] - '0'); }

        /// Builds a reply exactly the way ServePCMsg() fills TxBuffer.
        static byte[] FwResponse(byte id, byte cmd, byte status, byte pid, byte[] payload)
        {
            var b = new byte[5 + payload.Length + 2];
            b[0] = 0xFD; b[1] = id; b[2] = cmd; b[3] = status; b[4] = pid;
            Array.Copy(payload, 0, b, 5, payload.Length);
            b[b.Length - 2] = Fw.CalCRC(b, 1, b.Length - 3);
            b[b.Length - 1] = 0xFC;
            return b;
        }

        static byte[] RealtimeFrame(byte id, byte status, uint epoch, byte faults,
                                    float v1, float v2, float v3,
                                    float n1, float x1, float n2, float x2, float n3, float x3,
                                    byte a1, byte a2, byte a3)
        {
            var b = new byte[51];
            b[0] = 0xFD; b[1] = id; b[2] = 0x10; b[3] = status; b[4] = 0x48;
            Array.Copy(BitConverter.GetBytes(epoch), 0, b, 5, 4);
            b[9] = faults;
            int[] at = { 10, 14, 18, 22, 26, 30, 34, 38, 42 };
            float[] fv = { v1, v2, v3, n1, x1, n2, x2, n3, x3 };
            for (int i = 0; i < at.Length; i++)
                Array.Copy(BitConverter.GetBytes(fv[i]), 0, b, at[i], 4);
            b[46] = a1; b[47] = a2; b[48] = a3;
            b[49] = Fw.CalCRC(b, 1, 48);
            b[50] = 0xFC;
            return b;
        }
    }
}
