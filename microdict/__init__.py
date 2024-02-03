from enum import Enum
from _mdict_c import str_int64

class dtype(Enum):
    int32 = 1
    int64 = 2
    float32 = 3
    float64 = 4
    string = 5


def mdict(key_type, value_type):
    if key_type == dtype.string:
        if value_type == dtype.int64:
            return str_int64.create()
    raise NotImplementedError()
