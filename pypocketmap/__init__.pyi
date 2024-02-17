from enum import Enum
from typing import Literal, MutableMapping, Type, TypeVar, overload

class dtype(Enum):
    int32 = ...
    int64 = ...
    float32 = ...
    float64 = ...
    string = ...

int32_ = dtype.int32
int64_ = dtype.int64
float32_ = dtype.float32
float64_ = dtype.float64
string_ = dtype.string

_K = TypeVar("_K")
_V = TypeVar("_V")

class _Map(MutableMapping[_K, _V]):
    def copy(self) -> "_Map[_K, _V]":
        ...

@overload
def create(
    key_type: Literal[dtype.int32, dtype.int64] | Type[int],
    value_type: Literal[dtype.int32, dtype.int64] | Type[int],
) -> _Map[int, int]: ...
@overload
def create(
    key_type: Literal[dtype.int32, dtype.int64] | Type[int],
    value_type: Literal[dtype.float32, dtype.float64] | Type[float],
) -> _Map[int, float]: ...
@overload
def create(
    key_type: Literal[dtype.int32, dtype.int64] | Type[int],
    value_type: Literal[dtype.string] | Type[str],
) -> _Map[int, str]: ...
@overload
def create(
    key_type: Literal[dtype.float32, dtype.float64] | Type[float],
    value_type: Literal[dtype.int32, dtype.int64] | Type[int],
) -> _Map[float, int]: ...
@overload
def create(
    key_type: Literal[dtype.float32, dtype.float64] | Type[float],
    value_type: Literal[dtype.float32, dtype.float64] | Type[float],
) -> _Map[float, float]: ...
@overload
def create(
    key_type: Literal[dtype.float32, dtype.float64] | Type[float],
    value_type: Literal[dtype.string] | Type[str],
) -> _Map[float, str]: ...
@overload
def create(
    key_type: Literal[dtype.string] | Type[str],
    value_type: Literal[dtype.int32, dtype.int64] | Type[int],
) -> _Map[str, int]: ...
@overload
def create(
    key_type: Literal[dtype.string] | Type[str],
    value_type: Literal[dtype.float32, dtype.float64] | Type[float],
) -> _Map[str, float]: ...
@overload
def create(
    key_type: Literal[dtype.string] | Type[str],
    value_type: Literal[dtype.string] | Type[str],
) -> _Map[str, str]: ...
