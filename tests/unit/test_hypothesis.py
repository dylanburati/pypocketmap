from typing import Any, Callable, Generic, Protocol, TypeVar, overload
from typing_extensions import Self
from collections.abc import Collection, Mapping
import unittest

import deal
from hypothesis import given
import hypothesis.strategies as st
import pypocketmap as pkm

K = TypeVar("K", int, float, str)
V = TypeVar("V", int, float, str)
D = TypeVar("D")


class Map(Collection, Generic[K, V], Protocol):
    def __setitem__(self, __key: K, __value: V) -> None:
        ...

    def __delitem__(self, __key: K) -> None:
        ...

    @overload
    def pop(self, __key: K) -> V:
        ...

    @overload
    def pop(self, __key: K, __default: V) -> V:
        ...

    @overload
    def pop(self, __key: K, __default: D) -> V | D:
        ...

    def popitem(self) -> tuple[K, V]:
        ...

    def clear(self) -> None:
        ...

    # def update(self, other: Mapping[K, V]) -> None:
    #     ...
    @overload
    def setdefault(self, __key: K, __default: None = None) -> V | None:
        ...

    @overload
    def setdefault(self, __key: K, __default: V) -> V:
        ...

    def copy(self) -> Self:
        ...


def creator(
    item_type: tuple[pkm.dtype, pkm.dtype]
) -> Callable[[dict[Any, Any]], Map[Any, Any]]:
    kt, vt = item_type

    def func(src):
        res = pkm.create(kt, vt)  # type: ignore
        res.update(src)
        return res

    return func


def pocketmaps_strategy(
    item_type: tuple[pkm.dtype, pkm.dtype]
) -> st.SearchStrategy[Map[Any, Any]]:
    key_type, value_type = item_type
    strategy = st.dictionaries(type_strategy(key_type), type_strategy(value_type))
    return strategy.map(creator(item_type))


def type_strategy(dtype: pkm.dtype) -> st.SearchStrategy[Any]:
    if dtype == pkm.int32_:
        return st.integers(min_value=-(2**31), max_value=2**31 - 1)
    if dtype == pkm.int64_:
        return st.integers(min_value=-(2**63), max_value=2**63 - 1)
    if dtype == pkm.float32_:
        return st.floats(allow_nan=False, width=32)
    if dtype == pkm.float64_:
        return st.floats(allow_nan=False)
    if dtype == pkm.string_:
        return st.text()
    raise NotImplementedError()


@deal.ensure(lambda m, _k, _v, result: len(result) >= len(m))
@deal.ensure(lambda m, _k, _v, result: len(result) <= len(m) + 1)
@deal.ensure(lambda m, key, _v, result: set(result.keys()) == {key, *m.keys()})
@deal.ensure(lambda _m, key, val, result: result[key] == val)
def setitem(m: Map[K, V], key: K, val: V) -> Map[K, V]:
    res = m.copy()
    res[key] = val
    return res


@deal.reason(KeyError, lambda m, key: key not in m)
@deal.ensure(lambda m, _k, result: len(result) == len(m) - 1)
@deal.ensure(lambda m, key, result: set(result.keys()) == set(m.keys()) - {key})
def delitem(m: Map[K, V], key: K) -> Map[K, V]:
    m2 = m.copy()
    del m2[key]
    return m2


@deal.reason(KeyError, lambda m, key: key not in m)
@deal.ensure(lambda m, _k, result: len(result[0]) == len(m) - 1)
@deal.ensure(lambda m, key, result: set(result[0].keys()) == set(m.keys()) - {key})
@deal.ensure(lambda m, key, result: result[1] == m[key])
def pop_bang(m: Map[K, V], key: K) -> tuple[Map[K, V], V]:
    m2 = m.copy()
    val = m2.pop(key)
    return m2, val


@deal.ensure(lambda m, _k, _d, result: len(result[0]) <= len(m))
@deal.ensure(lambda m, key, _d, result: set(result[0].keys()) == set(m.keys()) - {key})
@deal.ensure(lambda m, key, default, result: result[1] == m.get(key, default))
def pop_default(m: Map[K, V], key: K, default: V) -> tuple[Map[K, V], V]:
    m2 = m.copy()
    val = m2.pop(key, default)
    return m2, val


@deal.reason(KeyError, lambda m: len(m) == 0)
@deal.ensure(lambda m, result: len(result[0]) == len(m) - 1)
@deal.ensure(lambda m, result: {result[1], *result[0].keys()} == set(m.keys()))
@deal.ensure(lambda m, result: m[result[1]] == result[2])
def popitem(m: Map[K, V]) -> tuple[Map[K, V], K, V]:
    m2 = m.copy()
    key, val = m2.popitem()
    return m2, key, val


@deal.ensure(lambda m, _k, _d, result: len(result[0]) >= len(m))
@deal.ensure(lambda m, _k, _d, result: len(result[0]) <= len(m) + 1)
@deal.ensure(lambda m, key, _d, result: set(result[0].keys()) == {key, *m.keys()})
@deal.ensure(lambda m, key, default, result: result[1] == m.get(key, default))
@deal.ensure(lambda m, key, default, result: result[0][key] == m.get(key, default))
def setdefault(m: Map[K, V], key: K, default: V) -> tuple[Map[K, V], V]:
    m2 = m.copy()
    val = m2.setdefault(key, default)
    return m2, val


@deal.ensure(lambda m, m2, result: len(result) >= len(m) and len(result) >= len(m2))
@deal.ensure(lambda m, m2, result: len(result) <= len(m) + len(m2))
@deal.ensure(lambda m, m2, result: set(result.keys()) == set(m.keys()).union(m2.keys()))
@deal.ensure(lambda _m, m2, result: all(result[k] == v2 for k, v2 in m2.items()))
@deal.ensure(
    lambda m, m2, result: all(result[k] == v for k, v in m.items() if k not in m2)
)
def update(m: Map[K, V], m2: Mapping[K, V]) -> Map[K, V]:
    m1 = m.copy()
    m1.update(m2)  # type: ignore
    return m1


class BaseHypothesisTest:
    # key_types = st.one_of(st.just(pkm.string_))
    # val_types = st.one_of(st.just(pkm.int64_), st.just(pkm.string_))
    # item_types = st.tuples(key_types, val_types)
    # pocketmaps = st.shared(item_types.flatmap(pocketmaps_strategy))

    item_type: tuple[pkm.dtype, pkm.dtype]

    def test_setitem(self):
        @given(
            m=pocketmaps_strategy(self.item_type),
            key=type_strategy(self.item_type[0]),
            val=type_strategy(self.item_type[1]),
        )
        def hypothesis_setitem(m, key, val):
            assert setitem(m, key, val) == setitem(dict(m), key, val)

        hypothesis_setitem()

    def test_delitem(self):
        @given(
            m=pocketmaps_strategy(self.item_type), key=type_strategy(self.item_type[0])
        )
        def hypothesis_delitem(m, key):
            if deal.catch(lambda: delitem(m, key)) is None:
                assert delitem(m, key) == delitem(dict(m), key)

        hypothesis_delitem()

    def test_pop_bang(self):
        @given(
            m=pocketmaps_strategy(self.item_type), key=type_strategy(self.item_type[0])
        )
        def hypothesis_pop_bang(m, key):
            if deal.catch(lambda: pop_bang(m, key)) is None:
                assert pop_bang(m, key) == pop_bang(dict(m), key)

        hypothesis_pop_bang()

    def test_pop_default(self):
        @given(
            m=pocketmaps_strategy(self.item_type),
            key=type_strategy(self.item_type[0]),
            val=type_strategy(self.item_type[1]),
        )
        def hypothesis_pop_default(m, key, val):
            assert pop_default(m, key, val) == pop_default(dict(m), key, val)

        hypothesis_pop_default()

    def test_popitem(self):
        @given(m=pocketmaps_strategy(self.item_type).filter(lambda d: len(d) > 0))
        def hypothesis_popitem(m):
            popitem(m)

        hypothesis_popitem()

    def test_popitem_empty(self):
        @given(m=st.builds(creator(self.item_type), st.just({})))
        def hypothesis_popitem_empty(m):
            try:
                popitem(m)
                assert False, "popitem should raise"
            except KeyError:
                pass

        hypothesis_popitem_empty()

    def test_setdefault(self):
        @given(
            m=pocketmaps_strategy(self.item_type),
            key=type_strategy(self.item_type[0]),
            val=type_strategy(self.item_type[1]),
        )
        def hypothesis_setdefault(m, key, val):
            assert setdefault(m, key, val) == setdefault(dict(m), key, val)

        hypothesis_setdefault()

    def test_update_selftype(self):
        @given(
            m=pocketmaps_strategy(self.item_type),
            m2=pocketmaps_strategy(self.item_type),
        )
        def hypothesis_update_selftype(m, m2):
            assert update(m, m2) == update(dict(m), m2)

        hypothesis_update_selftype()

    def test_update_dict(self):
        @given(
            m=pocketmaps_strategy(self.item_type),
            m2=pocketmaps_strategy(self.item_type).map(dict),
        )
        def hypothesis_update_dict(m, m2):
            assert update(m, m2) == update(dict(m), m2)

        hypothesis_update_dict()


class StrInt32HypothesisTest(BaseHypothesisTest, unittest.TestCase):
    item_type = (pkm.string_, pkm.int32_)


class StrInt64HypothesisTest(BaseHypothesisTest, unittest.TestCase):
    item_type = (pkm.string_, pkm.int64_)


class StrFloat32HypothesisTest(BaseHypothesisTest, unittest.TestCase):
    item_type = (pkm.string_, pkm.float32_)


class StrFloat64HypothesisTest(BaseHypothesisTest, unittest.TestCase):
    item_type = (pkm.string_, pkm.float64_)


class StrStrHypothesisTest(BaseHypothesisTest, unittest.TestCase):
    item_type = (pkm.string_, pkm.string_)
