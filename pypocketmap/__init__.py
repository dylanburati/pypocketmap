from enum import Enum
from _pkt_c import str_int64

class _dtype(Enum):
    int32 = 1
    int64 = 2
    float32 = 3
    float64 = 4
    string = 5


int32_ = _dtype.int32
int64_ = _dtype.int64
float32_ = _dtype.float32
float64_ = _dtype.float64
string_ = _dtype.string


def create(key_type, value_type):
    if key_type == string_ or key_type is str:
        if value_type == int64_ or value_type is int:
            return str_int64.create()
    raise NotImplementedError()
