# see https://github.com/dylanburati/pocketmap/blob/main/app/src/main/java/dev/dylanburati/App.java

import random
import sys


class Words:
    LENGTH_CDF = [
        2.55402880e-15, 3.73483535e-07, 2.06251620e-04, 4.60037401e-03,
        2.77018313e-02, 8.59221455e-02, 1.82026193e-01, 3.04121079e-01,
        4.34720260e-01, 5.58784740e-01, 6.67021855e-01, 7.55676596e-01,
        8.24886736e-01, 8.76934270e-01, 9.14931000e-01, 9.42014131e-01,
        9.60943967e-01, 9.73962076e-01, 9.82793792e-01, 9.88716864e-01,
        9.92650419e-01, 9.95240748e-01, 9.96934095e-01, 9.98034022e-01,
        9.98744497e-01, 9.99201152e-01, 9.99493382e-01, 9.99679661e-01,
        9.99797989e-01, 9.99872918e-01, 9.99920231e-01, 9.99950030e-01
    ]

    @staticmethod
    def gen_word_id(uniform):
        return int((-0.01 * 15.768233989819334 * uniform + 0.9870018865063785) ** -100)

    @staticmethod
    def gen_word_len(uniform):
        return next((i for i, x in enumerate(Words.LENGTH_CDF) if uniform < x), 32)

if __name__ == "__main__":
    implementation = sys.argv[0] if len(sys.argv) > 1 else ""
    if implementation == "dict":
        m = {}
    else:
        from microdict import mdict, dtype
        m = mdict(dtype.string, dtype.int64)

    random.seed(0)
    alph = "pfscxkde"     
    for i in range(100_000_000):
        uniform = random.random()
        wid = Words.gen_word_id(uniform)
        buf = []
        for j in range(Words.gen_word_len(uniform)):
            buf.append(alph[(wid >> (3 * (j%9))) & 7])
        w = "".join(buf)
        m[w] = m.get(w, 0) + 1
    print("Size:", len(m))