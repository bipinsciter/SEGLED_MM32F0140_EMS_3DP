# Protocol document tooling

`UART_Protocol.docx` in the parent folder is generated, not hand-written. Regenerating
it after a protocol change keeps it honest.

The parameter tables are not typed by hand: `extract.py` walks the read and write
dispatch switches in `main.c`, tracking the `#if DEVICE_MODE` guards, and records which
identifiers each switch handles plus the expression that produces each value. The
document is built from that, so what it claims a parameter does is what the code does
with it.

## Regenerating

```bash
python extract.py && python -c "import io,os;ns={'__name__':'__main__'};[exec(compile(io.open(p,encoding='utf-8').read(),p,'exec'),ns) for p in ('mkdoc2.py','mkdoc2b.py','mkdoc2c.py')]" && python checkdoc.py
```

Or the three steps separately:

1. `extract.py` — reads the firmware, writes `ids.json`.
2. `mkdoc2.py`, `mkdoc2b.py`, `mkdoc2c.py` — the document itself, in three parts,
   exec'd into one namespace. Part 1 is the preamble and frame formats, part 2 the
   parameter tables, part 3 the special frames and the closing sections.
3. `checkdoc.py` — reads the finished document back and checks every identifier the
   firmware implements is present, and that the access and build columns agree with
   the code. It also fails if a retired identifier is still listed as a parameter.

Requires `python-docx`.

`epoch_check.py` verifies `get_epoch_time()` against a reference for every date the
RTC can hold, and `tempcal_model.py` models the temperature calibration path as it is
against as it should be. Both are plain Python and need no hardware.

## When you add or change a parameter

Add it to `META` and to the right entry in `GROUPS` in `mkdoc2b.py`. Anything the
firmware implements but the groups do not mention is reported by `checkdoc.py` as
MISSING, so an omission will not slip through silently.
