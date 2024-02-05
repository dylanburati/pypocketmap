# Based on https://foss.heptapod.net/pypy/pypy/-/blob/ab3f173b52ba0b51b155c372af40de3d85a855a7/lib-python/2.7/test/test_dict.py
# Copyright: Python Software Foundation - https://docs.python.org/3/license.html

from collections.abc import KeysView, Mapping
import unittest

import pypocketmap as pkm

def pkm_of(d):
    res = pkm.create(str, int)
    res.update(d)
    return res

class PyPocketmapTest(unittest.TestCase):
    def test_constructor(self):
        # calling built-in types without argument must return empty
        self.assertEqual(pkm.create(str, int), {})

    def test_bool(self):
        self.assertIs(not pkm.create(str, int), True)
        self.assertTrue(pkm_of({'1': 2}))
        self.assertIs(bool(pkm.create(str, int)), False)
        self.assertIs(bool({'1': 2}), True)

    def test_keys(self):
        d = pkm.create(str, int)
        self.assertEqual(list(d.keys()), [])
        d = pkm_of({'a': 1, 'b': 2})
        self.assertEqual(set(d.keys()), {'a', 'b'})
        self.assertRaises(TypeError, d.keys, None)

    def test_values(self):
        d = pkm.create(str, int)
        self.assertEqual(list(d.values()), [])
        d = pkm_of({'1': 2})
        self.assertEqual(list(d.values()), [2])
        self.assertRaises(TypeError, d.values, None)

    def test_items(self):
        d = pkm.create(str, int)
        self.assertEqual(list(d.items()), [])
        d = pkm_of({'1': 2})
        self.assertEqual(list(d.items()), [('1', 2)])
        self.assertRaises(TypeError, d.items, None)

    def test_contains(self):
        d = pkm.create(str, int)
        self.assertNotIn('a', d)
        self.assertFalse('a' in d)
        self.assertTrue('a' not in d)
        d = pkm_of({'a': 1, 'b': 2})
        self.assertIn('a', d)
        self.assertIn('b', d)
        self.assertNotIn('c', d)
        self.assertRaises(TypeError, d.__contains__)
        self.assertRaises(TypeError, d.__contains__, 9)
        self.assertRaises(TypeError, d.__contains__, object())

    def test_len(self):
        d = pkm.create(str, int)
        self.assertEqual(len(d), 0)
        d = pkm_of({'a': 1, 'b': 2})
        self.assertEqual(len(d), 2)

    def test_getitem(self):
        d = pkm_of({'a': 1, 'b': 2})
        self.assertEqual(d['a'], 1)
        self.assertEqual(d['b'], 2)
        d['c'] = 3
        d['a'] = 4
        self.assertEqual(d['c'], 3)
        self.assertEqual(d['a'], 4)
        del d['b']
        self.assertEqual(d, {'a': 4, 'c': 3})
        self.assertRaises(TypeError, d.__getitem__)
        self.assertRaises(TypeError, d.__getitem__, 9)
        self.assertRaises(TypeError, d.__getitem__, object())

    def test_clear(self):
        d = pkm_of({'1': 1, '2': 2, '3': 3})
        d.clear()
        self.assertEqual(d, {})
        self.assertRaises(TypeError, d.clear, None)

    def test_update(self):
        d = pkm.create(str, int)
        d.update({'1': 100})
        d.update({'2': 20})
        d.update({'1': 1, '2': 2, '3': 3})
        self.assertEqual(d, {'1': 1, '2': 2, '3': 3})
        self.assertRaises(TypeError, d.update, {1: 2})
        # XXX: not an error in stdlib because of d.update(**kwargs)
        self.assertRaises(TypeError, d.update)
        # self.assertEqual(d, {1:1, 2:2, 3:3})

        self.assertRaises((TypeError, AttributeError), d.update, None)

        class SimpleUserDict(Mapping):
            def __init__(self):
                self.d = {1:1, 2:2, 3:3}
            def __iter__(self):
                return self.d.__iter__()
            def __len__(self):
                return len(self.d)
            def __getitem__(self, i):
                return self.d[i]
        d.clear()
        # XXX: this is not yet supported
        self.assertRaises(TypeError, d.update, SimpleUserDict())
        # self.assertEqual(d, {1:1, 2:2, 3:3})

        # XXX: this either
        # self.assertRaises(ValueError, d.update, [(1, 2, 3)])

        class Exc(Exception): pass

        d.clear()
        class FailingUserDict1(Mapping):
            def __iter__(self):
                raise Exc
            def __len__(self):
                return 1
            def __getitem__(self, key):
                return 0
        # XXX ""
        self.assertRaises(TypeError, d.update, FailingUserDict1())
        # self.assertRaises(Exc, d.update, FailingUserDict1())

        class FailingUserDict2(Mapping):
            def __iter__(self):
                class BogonIter(KeysView):
                    def __init__(self):
                        self.i = 1
                    def __iter__(self):
                        return self
                    def __next__(self):
                        if self.i:
                            self.i = 0
                            return 'a'
                        raise Exc
                return BogonIter()
            def __len__(self):
                return 2
            def __getitem__(self, key):
                return ord(key)
        # XXX ""
        self.assertRaises(TypeError, d.update, FailingUserDict2())
        # self.assertRaises(Exc, d.update, FailingUserDict2())

        class FailingUserDict3(Mapping):
            def __iter__(self):
                class BogonIter(KeysView):
                    def __init__(self):
                        self.i = ord('a')
                    def __iter__(self):
                        return self
                    def __next__(self):
                        if self.i <= ord('z'):
                            self.i += 1
                            return self.i - 1
                        raise StopIteration
                return BogonIter()
            def __len__(self):
                return 26
            def __getitem__(self, key):
                raise Exc
        # XXX ""
        self.assertRaises(TypeError, d.update, FailingUserDict3())
        # self.assertRaises(Exc, d.update, FailingUserDict3())


    def test_copy(self):
        d = pkm_of({'1': 1, '2': 2, '3': 3})
        self.assertEqual(d.copy(), {'1': 1, '2': 2, '3': 3})
        self.assertEqual(pkm.create(str, int).copy(), {})
        self.assertRaises(TypeError, d.copy, None)

    def test_get(self):
        d = pkm.create(str, int)
        self.assertIs(d.get('c'), None)
        self.assertEqual(d.get('c', 3), 3)
        d = {'a': 1, 'b': 2}
        self.assertIs(d.get('c'), None)
        self.assertEqual(d.get('c', 3), 3)
        self.assertEqual(d.get('a'), 1)
        self.assertEqual(d.get('a', 3), 1)
        self.assertRaises(TypeError, d.get)
        # TODO: these just return the default, should it error?
        # self.assertRaises(TypeError, d.get, None)
        # self.assertRaises(TypeError, d.get, 9)
        # self.assertRaises(TypeError, d.get, object())
        self.assertRaises(TypeError, d.get, None, None, None)

    def test_setdefault(self):
        d = pkm.create(str, int)
        self.assertIs(d.setdefault('key0'), 0)  # type: ignore
        d.setdefault('key0', 100)
        self.assertIs(d.setdefault('key0'), 0)  # type: ignore
        self.assertIs(d.setdefault('key1', 1), 1)
        self.assertRaises(TypeError, d.setdefault)

    def test_popitem(self):
        # dict.popitem()
        for log2size in range(12):
            size = 2**log2size
            a = pkm.create(str, int)
            b = pkm.create(str, int)
            for i in range(size):
                a[repr(i)] = i
            b = a.copy()
            for i in range(size):
                ka, va = a.popitem()
                self.assertEqual(va, int(ka))
                kb, vb = b.popitem()
                self.assertEqual(vb, int(kb))
            self.assertFalse(a)
            self.assertFalse(b)
            self.assertRaises(KeyError, a.popitem)
            self.assertRaises(KeyError, b.popitem)

        d = pkm.create(str, int)
        self.assertRaises(KeyError, d.popitem)

    def test_pop(self):
        # Tests for pop with specified key
        d = pkm.create(str, int)
        k, v = 'abc', 456
        d[k] = v
        self.assertRaises(KeyError, d.pop, 'ghi')
        self.assertEqual(d.pop(k), v)
        self.assertEqual(len(d), 0)
        self.assertRaises(KeyError, d.pop, k)

        self.assertEqual(d.pop(k, v), v)
        d[k] = v
        self.assertEqual(d.pop(k, 1), v)
        self.assertEqual(d.pop(k, 1), 1)
        self.assertEqual(d.pop(k, '1'), '1')

        self.assertRaises(TypeError, d.pop)

    @unittest.skip("todo")
    def test_mutatingiteration(self):
        # changing dict size during iteration
        d = pkm.create(str, int)
        d['1'] = 1
        with self.assertRaises(RuntimeError):
            for i in d:
                d[i+'1'] = 1

    def test_repr(self):
        d = pkm.create(str, int)
        self.assertEqual(repr(d), '<pypocketmap[str, int64]: {}>')
        d['1'] = 2
        self.assertEqual(repr(d), "<pypocketmap[str, int64]: {'1': 2}>")

    def test_ord_comparisons(self):
        a = pkm.create(str, int)
        b = {}
        with self.assertRaises(TypeError):
            print(a < b)  # type: ignore
        with self.assertRaises(TypeError):
            print(a <= b)  # type: ignore
        with self.assertRaises(TypeError):
            print(a >= b)  # type: ignore
        with self.assertRaises(TypeError):
            print(a > b)  # type: ignore

    def test_missing(self):
        self.assertFalse(hasattr(pkm.create(str, int), "__missing__"))

    def test_tuple_keyerror(self):
        d = pkm.create(str, int)
        with self.assertRaises(TypeError):
            d[(1,)]  # type: ignore

    def test_resize1(self):
        # Dict resizing bug, found by Jack Jansen in 2.2 CVS development.
        # This version got an assert failure in debug build, infinite loop in
        # release build.  Unfortunately, provoking this kind of stuff requires
        # a mix of inserts and deletes hitting exactly the right hash codes in
        # exactly the right order, and I can't think of a randomized approach
        # that would be *likely* to hit a failing case in reasonable time.

        d = pkm.create(str, int)
        for i in range(5):
            d[repr(i)] = i
        for i in range(5):
            del d[repr(i)]
        for i in range(5, 9):  # i==8 was the problem
            d[repr(i)] = i

    # def _check_free_after_iterating(self, ctor, to_iter, msg):
        # def iterator_and_ref():
            # tmp = ctor()
            # it = to_iter(tmp)
            # return it, weakref.ref(tmp)
        # it, r = iterator_and_ref()
        # for _ in it:
            # pass
        # gc.collect()
        # self.assertIs(r(), None, msg)

    # def test_free_after_iterating(self):
        # self._check_free_after_iterating(lambda: pkm.create(str, int), iter, "iter")
        # self._check_free_after_iterating(lambda: pkm.create(str, int), lambda d: d.keys(), "keys")
        # self._check_free_after_iterating(lambda: pkm.create(str, int), lambda d: d.values(), "values")
        # self._check_free_after_iterating(lambda: pkm.create(str, int), lambda d: d.items(), "items")

    def test_overflow(self):
        d = pkm.create(str, int)
        bot, top = -(2 ** 63), (2 ** 63) - 1
        d["+"] = top
        d["-"] = bot
        self.assertEqual(d["+"], top)
        self.assertEqual(d["-"], bot)
        for k in ("+", "++"):
            with self.assertRaises(OverflowError):
                d[k] = top + 1
        self.assertRaises(OverflowError, d.setdefault, "++", top + 1)
        self.assertRaises(OverflowError, d.update, {"+": top + 1})
        self.assertEqual(d.pop("+"), top)
        for k in ("-", "--"):
            with self.assertRaises(OverflowError):
                d[k] = bot - 1
        self.assertRaises(OverflowError, d.setdefault, "--", bot - 1)
        self.assertRaises(OverflowError, d.update, {"-": bot - 1})
        self.assertEqual(d.pop("-"), bot)
