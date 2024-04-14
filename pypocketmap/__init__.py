from enum import Enum
from _pkt_c import int64_int64, str_float32, str_float64, str_int32, str_int64, str_str

class dtype(Enum):
    int32 = 1
    int64 = 2
    float32 = 3
    float64 = 4
    string = 5


int32_ = dtype.int32
int64_ = dtype.int64
float32_ = dtype.float32
float64_ = dtype.float64
string_ = dtype.string


def create(key_type, value_type):
    if key_type == string_ or key_type is str:
        if value_type == int32_:
            return str_int32.create()
        if value_type == int64_ or value_type is int:
            return str_int64.create()
        if value_type == float32_:
            return str_float32.create()
        if value_type == float64_ or value_type is float:
            return str_float64.create()
        if value_type == string_ or value_type is str:
            return str_str.create()
    if key_type == int64_ or key_type is int:
        if value_type == int64_ or value_type is int:
            return int64_int64.create()
    raise NotImplementedError()
