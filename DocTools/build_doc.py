import io, os
dt = r"D:\TechnicalData\Yagneshbhai\PRODUCT_CODE\xmegabased\ReleaseCode\SEGLED_MM32F0140_EMS_3DP\DocTools"
ns = {"__file__": os.path.join(dt, "mkdoc2.py"), "__name__": "__main__"}
for part in ("mkdoc2.py", "mkdoc2b.py", "mkdoc2c.py"):
    src = io.open(os.path.join(dt, part), encoding="utf-8").read()
    exec(compile(src, part, "exec"), ns)
