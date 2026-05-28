import sys
import bisect

addrs = [0x400e5f00, 0x400e65fe, 0x400e603b, 0x400e6453, 0x40083a11, 0x4008748d]
syms = []
try:
    with open('syms.txt', 'r', encoding='utf-16') as f:
        lines = f.readlines()
except:
    with open('syms.txt', 'r', encoding='utf-8') as f:
        lines = f.readlines()

for line in lines:
    parts = line.strip().split()
    if len(parts) >= 3:
        try:
            addr = int(parts[0], 16)
            syms.append((addr, " ".join(parts[2:])))
        except:
            pass

syms.sort()
addrs_keys = [s[0] for s in syms]

for a in addrs:
    idx = bisect.bisect_right(addrs_keys, a) - 1
    if idx >= 0:
        print(f"{hex(a)}: {syms[idx][1]} + {hex(a - syms[idx][0])}")
