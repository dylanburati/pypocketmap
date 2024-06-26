import numpy as np
import random as random
import sys


if __name__ == "__main__":
    implementation = sys.argv[1] if len(sys.argv) > 1 else ""
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 5_000_000
    if implementation == "dict":
        m = {}
    else:
        import pypocketmap
        m = pypocketmap.create(str, int)
    print(m)
    rng = np.random.default_rng(0)

    scale = 2.5
    lanes = (
        ((rng.exponential(scale, (100, 100)).astype(np.uint32) | 64) << 24)
        | ((rng.exponential(scale, (100, 100)).astype(np.uint32) | 64) << 16)
        | ((rng.exponential(scale, (100, 100)).astype(np.uint32) | 64) << 8)
        | (rng.exponential(scale, (100, 100)).astype(np.uint32) | 64)
    )
    i = 0
    while i < iterations:
        b = lanes.tobytes()
        st = 0
        lx = np.cumsum(np.pad(2 + rng.poisson(8, 4000), (1, 0)))
        for j in range(4000):
            en = lx[j+1]
            if en > len(b):
                break
            w = b[lx[j]:en].decode("ascii")
            m[w] = m.get(w, 0) + 1
            i += 1
        vl = lanes.view()
        vl.shape = (10000,)
        rng.shuffle(vl)
    print("Size:", len(m))
