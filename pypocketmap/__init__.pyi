from enum import Enum
from typing import Literal, MutableMapping, Type, overload

class _dtype(Enum):
    int32 = ...
    int64 = ...
    float32 = ...
    float64 = ...
    string = ...

int32_ = _dtype.int32
int64_ = _dtype.int64
float32_ = _dtype.float32
float64_ = _dtype.float64
string_ = _dtype.string

@overload
def create(
    key_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
    value_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
) -> MutableMapping[int, int]: ...
@overload
def create(
    key_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
    value_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
) -> MutableMapping[int, float]: ...
@overload
def create(
    key_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
    value_type: Literal[_dtype.string] | Type[str],
) -> MutableMapping[int, str]: ...
@overload
def create(
    key_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
    value_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
) -> MutableMapping[float, int]: ...
@overload
def create(
    key_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
    value_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
) -> MutableMapping[float, float]: ...
@overload
def create(
    key_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
    value_type: Literal[_dtype.string] | Type[str],
) -> MutableMapping[float, str]: ...
@overload
def create(
    key_type: Literal[_dtype.string] | Type[str],
    value_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
) -> MutableMapping[str, int]: ...
@overload
def create(
    key_type: Literal[_dtype.string] | Type[str],
    value_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
) -> MutableMapping[str, float]: ...
@overload
def create(
    key_type: Literal[_dtype.string] | Type[str],
    value_type: Literal[_dtype.string] | Type[str],
) -> MutableMapping[str, str]: ...
