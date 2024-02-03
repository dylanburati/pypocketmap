import math
import random
import sys


if __name__ == "__main__":
    implementation = sys.argv[1] if len(sys.argv) > 1 else ""
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 5_000_000
    if implementation == "dict":
        m = {}
    else:
        from microdict import mdict, dtype
        m = mdict(dtype.string, dtype.int64)
    print(m)

    random.seed(0)
    translations = []
    for sz in [2, 3, 5, 7, 11, 13, 17, 19]:
        t = bytes(range(sz)) * ((256+sz-1)//sz)
        translations.append(t[:256])
    for i in range(iterations):
        uniform = random.random()
        b = random.randbytes(int(-math.log2(1e-3 + 0.1 * uniform)))
        b = b.translate(translations[int(uniform * 4096) & 7])
        w = b.hex()
        m[w] = m.get(w, 0) + 1
    print("Size:", len(m))
