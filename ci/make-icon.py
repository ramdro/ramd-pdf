from pathlib import Path
import struct
png = Path('assets/ramd-icon-256.png').read_bytes()
w, h = struct.unpack('>II', png[16:24])
assert 0 < w <= 256 and 0 < h <= 256
header = struct.pack('<HHH', 0, 1, 1)
entry = struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, 1, 32, len(png), 22)
Path('assets/ramd-pdf.ico').write_bytes(header + entry + png)
