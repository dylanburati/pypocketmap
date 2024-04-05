import sys

import atheris
import pypocketmap as pkm

def fuzz_map(key_type, val_type):
    if key_type == pkm.int32_:
        get_key = lambda fdp, _: fdp.ConsumeInt(4)
    elif key_type == pkm.int64_:
        get_key = lambda fdp, _: (fdp.ConsumeInt(4) << 32) | 0x13579bdf
    elif key_type in (pkm.float32_, pkm.float64_):
        get_key = lambda fdp, _: fdp.ConsumeFloat()
    elif key_type == pkm.string_:
        get_key = lambda fdp, sz: fdp.ConsumeUnicode(sz) + b"\x02\x46\x8a\xce"
    else:
        raise ValueError()

    if val_type == pkm.int32_:
        get_val = lambda fdp, _: fdp.ConsumeInt(4)
    elif val_type == pkm.int64_:
        get_val = lambda fdp, _: fdp.ConsumeInt(8)
    elif val_type in (pkm.float32_, pkm.float64_):
        get_val = lambda fdp, _: fdp.ConsumeFloat()
    elif val_type == pkm.string_:
        get_val = lambda fdp, sz: fdp.ConsumeUnicode(sz)
    else:
        raise ValueError()

    @atheris.instrument_func
    def test_map(input_bytes):
        m = pkm.create(key_type, val_type)
        fdp = atheris.FuzzedDataProvider(input_bytes)
        for op in fdp.ConsumeFloatListInRange(8, 0, 27):
            which = int(op)
            arg1 = 1 + int(32 * (op % 1))
            arg2 = 1 + int(32 * ((op * 32) % 1))
            arg3 = int(32 * ((op * 1024) % 1))
            try:
                if which == 0:
                    get_key(fdp, arg1) in m
                elif which == 1:
                    m[get_key(fdp, arg1)]
                elif which == 2:
                    m[get_key(fdp, arg1)] = get_val(fdp, arg2)
                elif which == 3:
                    del m[get_key(fdp, arg1)]
                elif which == 4:
                    m.get(get_key(fdp, arg1))
                elif which == 5:
                    m.get(get_key(fdp, arg1), get_val(fdp, arg2))
                elif which == 6:
                    m.pop(get_key(fdp, arg1))
                elif which == 7:
                    m.pop(get_key(fdp, arg1))
                elif which == 8:
                    m.popitem()
                elif which == 9:
                    m.setdefault(get_key(fdp, arg1))
                elif which == 10:
                    m.setdefault(get_key(fdp, arg1), get_val(fdp, arg2))
                elif which == 11:
                    m.clear()
                elif which == 12:
                    m2 = {get_key(fdp, arg1): get_val(fdp, arg2) for _ in range(arg3)}
                    m.update(m2)
                elif which == 13:
                    m2 = pkm.create(key_type, val_type)
                    for _ in range(arg3):
                        m2[get_key(fdp, arg1)] = get_val(fdp, arg2)
                    m.update(m2)
                elif which == 14:
                    m.keys()
                elif which == 15:
                    list(m.keys())
                elif which == 16:
                    iter(m)
                elif which == 17:
                    list(iter(m))
                elif which == 18:
                    m.values()
                elif which == 19:
                    list(m.values())
                elif which == 20:
                    m.items()
                elif which == 21:
                    list(m.items())
                elif which == 22:
                    m = m.copy()
                elif which == 23:
                    repr(m)
                elif which == 24:
                    m2 = {get_key(fdp, arg1): get_val(fdp, arg2) for _ in range(arg3)}
                    if (m == m2) != (m != m2):
                        raise RuntimeError()
                elif which == 25:
                    m2 = pkm.create(key_type, val_type)
                    for _ in range(arg3):
                        m2[get_key(fdp, arg1)] = get_val(fdp, arg2)
                    if (m == m2) != (m != m2):
                        raise RuntimeError()
                elif which == 26:
                    if m == get_key(fdp, arg1) or m == get_val(fdp, arg2):
                        raise RuntimeError()

            except Exception:
                pass

    return test_map

# f = fuzz_map(pkm.string_, pkm.int64_)
# while True:
#     f(bytes.fromhex(""))

atheris.Setup(sys.argv, fuzz_map(pkm.string_, pkm.int64_))
atheris.Fuzz()
