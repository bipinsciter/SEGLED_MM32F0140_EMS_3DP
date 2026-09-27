// Reading the device's five logs, and turning them into a report.
//
// Four of the five are STREAMED: the request arms a transfer and the device then
// pushes one frame per main-loop pass until its count is exhausted. So a read is one
// request followed by however many frames arrive, not a request and a single reply.
// RAM_ALL is the exception - one 1507 byte reply carrying all thirty records.
//
// Frame shapes, taken from the senders in main.c. The record offset differs between
// them, which is easy to get wrong: the regular log has no index byte, the RAM frames
// have one, and the 24 hour ring has two.
//
//   log                 cmd   frame   record at   record
//   regular log         0x49   70       5          50 bytes
//   24 hour ring        0x47   72       7          50 bytes   (index at [5..6], big endian)
//   15 day min/max/mean 0x53   25       7          16 bytes
//   24 hourly means     0x54   13       7           4 bytes   (one float)
//   RAM buffer, bulk    0x45 1507       5 + n*50   50 bytes   (30 records)
//   RAM buffer, one     0x46   71       6          50 bytes
//
// A 50 byte record cannot be found by scanning for the 0xFC terminator, because its
// floats contain that byte often enough. Frames are taken by their fixed length.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;

namespace NiyamaConfig
{
    // ---------------------------------------------------------------- records

    /// One 50 byte reading record, as written by LogReading() and FillRamBuffer().
    class LogRecord
    {
        public byte Device, RtcSet, Faults, Alarm1, Alarm2, Alarm3;
        public DateTime? Stamp;
        public float V1, V2, V3, Min1, Max1, Min2, Max2, Min3, Max3;
        public bool Fahrenheit, Written;

        /// The ring and RAM buffer layout, written by FillRamBuffer():
        ///
        ///   [0..3]   epoch, uint32 little endian
        ///   [4]      log type
        ///   [5]      user id
        ///   [6]      spare
        ///   [7..8]   password
        ///   [9]      sensor fault flags
        ///   [10..13] DP1                      float
        ///   [14..17] DP2 or temperature       float
        ///   [18..21] DP3 or humidity          float
        ///   [22..29] DP1 minimum, maximum
        ///   [30..37] DP2/temperature min, max
        ///   [38..45] DP3/humidity min, max
        ///   [46..48] the three alarm states
        ///
        /// There is no size marker here, unlike the regular log, so a slot is judged
        /// written by whether its epoch is plausible.
        public static LogRecord FromRamBuffer(byte[] b, int off)
        {
            var r = new LogRecord();
            if (off + 50 > b.Length) return r;

            uint epoch = BitConverter.ToUInt32(b, off);
            // an erased slot reads as all ones or all zeros; anything before 2020 here
            // is not a real timestamp
            if (epoch == 0 || epoch == 0xFFFFFFFF || epoch < 1577836800u) return r;

            r.Written = true;
            r.Stamp = new DateTime(1970, 1, 1).AddSeconds(epoch);
            r.RtcSet = 1;                      // it could not have been written otherwise
            //The password occupies [7..8] - reading the faults from [8] picked up its
            //high byte, which for the 0xFFFF a normal entry carries lit every flag.
            r.Faults = b[off + 9];
            r.V1 = BitConverter.ToSingle(b, off + 10);
            r.V2 = BitConverter.ToSingle(b, off + 14);
            r.V3 = BitConverter.ToSingle(b, off + 18);
            r.Min1 = BitConverter.ToSingle(b, off + 22);
            r.Max1 = BitConverter.ToSingle(b, off + 26);
            r.Min2 = BitConverter.ToSingle(b, off + 30);
            r.Max2 = BitConverter.ToSingle(b, off + 34);
            r.Min3 = BitConverter.ToSingle(b, off + 38);
            r.Max3 = BitConverter.ToSingle(b, off + 42);
            if (off + 49 <= b.Length)
            {
                r.Alarm1 = b[off + 46];
                r.Alarm2 = (byte)(b[off + 47] & 0x7F);
                r.Fahrenheit = (b[off + 47] & 0x80) != 0;
                r.Alarm3 = b[off + 48];
            }
            return r;
        }

        /// The regular log layout, written by LogReading() from Buffer1.
        public static LogRecord Parse(byte[] b, int off)
        {
            var r = new LogRecord();
            // every written slot starts with the record size; erased flash reads 0xFF
            if (off + 50 > b.Length || b[off] != 50) return r;
            r.Written = true;

            r.Device = b[off + 1];
            r.RtcSet = b[off + 3];
            int hh = b[off + 4], mi = b[off + 5], ss = b[off + 6];
            int dd = b[off + 7], mo = b[off + 8], yy = b[off + 9];
            if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 && hh < 24 && mi < 60 && ss < 60)
            {
                try { r.Stamp = new DateTime(2000 + yy, mo, dd, hh, mi, ss); }
                catch (ArgumentOutOfRangeException) { }
            }

            r.Faults = b[off + 10];
            r.V1 = BitConverter.ToSingle(b, off + 11);
            r.V2 = BitConverter.ToSingle(b, off + 15);
            r.V3 = BitConverter.ToSingle(b, off + 19);
            r.Min1 = BitConverter.ToSingle(b, off + 23);
            r.Max1 = BitConverter.ToSingle(b, off + 27);
            r.Min2 = BitConverter.ToSingle(b, off + 31);
            r.Max2 = BitConverter.ToSingle(b, off + 35);
            r.Min3 = BitConverter.ToSingle(b, off + 39);
            r.Max3 = BitConverter.ToSingle(b, off + 43);
            r.Alarm1 = b[off + 47];
            r.Alarm2 = (byte)(b[off + 48] & 0x7F);
            r.Fahrenheit = (b[off + 48] & 0x80) != 0;   // the unit on DISPLAY at the time
            r.Alarm3 = b[off + 49];
            return r;
        }
    }

    /// One day of the 15 day archive: a date, then the minimum, maximum and mean.
    class DayRecord
    {
        public DateTime? Day;
        public float Min, Max, Mean;
        public bool Written;

        public static DayRecord Parse(byte[] b, int off)
        {
            var r = new DayRecord();
            if (off + 16 > b.Length) return r;
            uint epoch = BitConverter.ToUInt32(b, off);
            //Never-written flash decodes to timestamps in the 1970s and values with
            //twenty digits. Anything before 2020 did not come from this instrument.
            if (epoch == 0 || epoch == 0xFFFFFFFF || epoch < 1577836800u) return r;

            float mn = BitConverter.ToSingle(b, off + 4);
            float mx = BitConverter.ToSingle(b, off + 8);
            float av = BitConverter.ToSingle(b, off + 12);
            if (Bad(mn) || Bad(mx) || Bad(av)) return r;

            r.Written = true;
            r.Day = new DateTime(1970, 1, 1).AddSeconds(epoch);
            r.Min = mn;
            r.Max = mx;
            r.Mean = av;
            return r;
        }

        /// Readings this instrument cannot produce: its sensors span a few hundred Pa
        /// and a hundred degrees, so anything wilder is unwritten flash.
        static bool Bad(float v)
        {
            return float.IsNaN(v) || float.IsInfinity(v) || Math.Abs(v) > 10000f;
        }
    }

    // ---------------------------------------------------------------- reader

    static class Logs
    {
        public const byte ID_RDLG_DT = 0x49, ID_RDLG_CNT = 0x4A;
        public const byte ID_FLASH24 = 0x47, ID_FLASH24_CUR = 0x4C;
        public const byte ID_MINMAX = 0x53, ID_MEANHR = 0x54;
        public const byte ID_RAM_ALL = 0x45;

        public const int RAM_ALL_LEN = 1507, RAM_RECORDS = 30, RAM_FILL_START = 5;

        /// Send one request, then gather whatever frames come back.
        ///
        /// Frames are taken at a fixed length because record data contains 0xFC. The
        /// device may lead with a short ASCII frame (the regular log announces its
        /// record count that way), so that shape is accepted too.
        public static List<byte[]> Stream(SerialPort port, byte[] request,
                                          int frameLen, int quietMs, int capMs,
                                          Action<string> log)
        {
            var frames = new List<byte[]>();
            try
            {
                System.Threading.Thread.Sleep(120);
                port.DiscardInBuffer();
                port.Write(request, 0, request.Length);
            }
            catch (Exception ex) { log("write failed: " + ex.Message); return frames; }

            var rx = new List<byte>();
            var start = DateTime.UtcNow;
            var last = DateTime.MinValue;
            while ((DateTime.UtcNow - start).TotalMilliseconds < capMs)
            {
                if (port.BytesToRead > 0)
                {
                    var buf = new byte[port.BytesToRead];
                    int n = port.Read(buf, 0, buf.Length);
                    for (int i = 0; i < n; i++) rx.Add(buf[i]);
                    last = DateTime.UtcNow;
                }
                else
                {
                    if (rx.Count > 0 && last != DateTime.MinValue
                        && (DateTime.UtcNow - last).TotalMilliseconds > quietMs) break;
                    System.Threading.Thread.Sleep(4);
                }
            }

            var d = rx.ToArray();
            int p = 0;
            while (p < d.Length)
            {
                if (d[p] != 0xFD) { p++; continue; }

                if (frameLen > 0 && p + frameLen <= d.Length
                    && d[p + frameLen - 1] == 0xFC
                    && Proto.Crc(d, p + 1, frameLen - 3) == d[p + frameLen - 2])
                {
                    var f = new byte[frameLen];
                    Array.Copy(d, p, f, 0, frameLen);
                    frames.Add(f);
                    p += frameLen;
                    continue;
                }

                bool got = false;
                for (int j = p + 6; j < Math.Min(d.Length, p + 28); j++)
                {
                    if (d[j] != 0xFC) continue;
                    int len = j - p + 1;
                    if (Proto.Crc(d, p + 1, len - 3) != d[j - 1]) continue;
                    var f = new byte[len];
                    Array.Copy(d, p, f, 0, len);
                    frames.Add(f);
                    p = j + 1;
                    got = true;
                    break;
                }
                if (!got) p++;
            }
            return frames;
        }

        static byte[] Build(byte devId, byte cmd, byte pid, byte[] payload)
        {
            var body = new List<byte> { devId, cmd, pid };
            if (payload != null) body.AddRange(payload);
            var f = new List<byte> { (byte)0xFF };
            f.AddRange(body);
            var arr = body.ToArray();
            f.Add(Proto.Crc(arr, 0, arr.Length));
            f.Add(0xFE);
            return f.ToArray();
        }

        // ---- the five reads

        public static List<LogRecord> ReadRegular(SerialPort port, byte devId,
                                                  DateTime from, DateTime to,
                                                  Action<string> log, out int reported)
        {
            reported = -1;
            var payload = new List<byte>();
            payload.AddRange(Stamp(from));
            payload.AddRange(Stamp(to));

            //This one follows whatever transfer ran before it, and the device needs a
            //moment between them; without the pause it sometimes never starts sending.
            System.Threading.Thread.Sleep(600);
            var frames = Stream(port, Build(devId, Proto.CMD_READ, ID_RDLG_DT, payload.ToArray()),
                                70, 1200, 60000, log);

            var outp = new List<LogRecord>();
            foreach (var f in frames)
            {
                if (f.Length < 70)
                {
                    // The device leads with a short frame giving how many records fall
                    // in the window, as a uint32 - not ASCII like the ordinary replies.
                    if (f.Length >= 11) reported = (int)BitConverter.ToUInt32(f, 5);
                    continue;
                }

                // Records here carry the same epoch-first layout the ring and the RAM
                // buffer use; the regular log is not a different shape after all.
                var r = LogRecord.FromRamBuffer(f, 5);
                if (r.Written) outp.Add(r);
            }
            return outp;
        }

        public static List<LogRecord> ReadRing(SerialPort port, byte devId, int count,
                                               Action<string> log)
        {
            if (count < 1) count = 1;
            if (count > 1440) count = 1440;
            var payload = Encoding.ASCII.GetBytes("0000" + count.ToString("D4"));
            var frames = Stream(port, Build(devId, Proto.CMD_READ, ID_FLASH24, payload),
                                72, 700, 60000, log);

            var outp = new List<LogRecord>();
            foreach (var f in frames)
            {
                if (f.Length < 72) continue;
                var r = LogRecord.FromRamBuffer(f, 7);   // two index bytes first
                if (r.Written) outp.Add(r);
            }
            return outp;
        }

        public static List<LogRecord> ReadRam(SerialPort port, byte devId, Action<string> log)
        {
            var frames = Stream(port, Build(devId, Proto.CMD_READ, ID_RAM_ALL, null),
                                RAM_ALL_LEN, 500, 6000, log);
            var outp = new List<LogRecord>();
            if (frames.Count == 0) return outp;
            var f = frames[0];
            for (int i = 0; i < RAM_RECORDS; i++)
            {
                var r = LogRecord.FromRamBuffer(f, RAM_FILL_START + i * 50);
                if (r.Written) outp.Add(r);
            }
            return outp;
        }

        public static List<DayRecord> ReadDays(SerialPort port, byte devId, int channel,
                                               int count, Action<string> log)
        {
            if (count < 1) count = 1;
            if (count > 15) count = 15;
            var payload = Encoding.ASCII.GetBytes(
                              ((char)('0' + channel)).ToString() + count.ToString("D2"));
            var frames = Stream(port, Build(devId, Proto.CMD_READ, ID_MINMAX, payload),
                                25, 600, 12000, log);

            var outp = new List<DayRecord>();
            foreach (var f in frames)
            {
                if (f.Length < 25) continue;
                var r = DayRecord.Parse(f, 7);
                if (r.Written) outp.Add(r);
            }
            return outp;
        }

        public static List<float> ReadHourlyMeans(SerialPort port, byte devId, int channel,
                                                  Action<string> log)
        {
            var payload = Encoding.ASCII.GetBytes(((char)('0' + channel)).ToString());
            var frames = Stream(port, Build(devId, Proto.CMD_READ, ID_MEANHR, payload),
                                13, 700, 12000, log);

            var outp = new List<float>();
            foreach (var f in frames)
            {
                if (f.Length < 13) continue;
                outp.Add(BitConverter.ToSingle(f, 7));
            }
            return outp;
        }

        static byte[] Stamp(DateTime t)
        {
            return new byte[] { (byte)t.Day, (byte)t.Month, (byte)(t.Year - 2000),
                                (byte)t.Hour, (byte)t.Minute, (byte)t.Second };
        }
    }
}
