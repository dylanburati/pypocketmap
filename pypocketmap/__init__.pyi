from enum import Enum
from typing import Literal, MutableMapping, Type, TypeVar, overload

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

_K = TypeVar("_K")
_V = TypeVar("_V")

class _Map(MutableMapping[_K, _V]):
    def copy(self) -> "_Map[_K, _V]":
        ...

@overload
def create(
    key_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
    value_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
) -> _Map[int, int]: ...
@overload
def create(
    key_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
    value_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
) -> _Map[int, float]: ...
@overload
def create(
    key_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
    value_type: Literal[_dtype.string] | Type[str],
) -> _Map[int, str]: ...
@overload
def create(
    key_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
    value_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
) -> _Map[float, int]: ...
@overload
def create(
    key_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
    value_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
) -> _Map[float, float]: ...
@overload
def create(
    key_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
    value_type: Literal[_dtype.string] | Type[str],
) -> _Map[float, str]: ...
@overload
def create(
    key_type: Literal[_dtype.string] | Type[str],
    value_type: Literal[_dtype.int32, _dtype.int64] | Type[int],
) -> _Map[str, int]: ...
@overload
def create(
    key_type: Literal[_dtype.string] | Type[str],
    value_type: Literal[_dtype.float32, _dtype.float64] | Type[float],
) -> _Map[str, float]: ...
@overload
def create(
    key_type: Literal[_dtype.string] | Type[str],
    value_type: Literal[_dtype.string] | Type[str],
) -> _Map[str, str]: ...
