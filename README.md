# pypocketmap

NOTE: this package is in alpha. The current repo only contains
`pypocketmap[str, int64]`, and I don't know how long it will be before
I add support for the other key/value type combinations.

A high performance python hash table library that consumes significantly less
memory than Python Dictionaries. It currently supports Python 3.6+. It is forked from
[Microdict](https://github.com/touqir14/Microdict) to remove the limitation on the
length of string keys, and add utility methods like `get` and `setdefault`.
It also has a slightly different table design, making use of code ported to
C from [abseil-cpp](https://github.com/abseil/abseil-cpp).

### Benchmarks
The latest charts are at https://observablehq.com/@dylan-burati-ws/pypocketmap-benchmarks

### Installation and Building
You can install Microdict using pip: `pip install pypocketmap`.

Microdict is tested to work on Linux, Mac OSX, and Windows systems. You will need
GCC 7+ on linux/mac osx systems and Visual C++ 14+ compiler on Windows systems to
build the package. For the best performance use on a 64 bit system.

### Usage
The following code snippet shows common uses of the library.

```python
>>> import pypocketmap as pkm

# Generates a dictionary with string keys and signed 64 bit integer values.
>>> d = pkm.create(str, int)
>>> d = pkm.create(pkm.string, pkm.i64)  # or explicitly

# Works just like a python dictionary, although insertion order != iteration order
>>> d["a"] = 2
>>> "a" in d, "b" in d
(True, False)

# `get` default value can be any type
>>> d.get("a"), d.get("b"), d.get("b", False)
(2, None, False)

# `setdefault` default value must be an int
>>> d.setdefault("a", -10)
2
>>> d.setdefault("b", -10)
-10
>>> d.update({"okc": 1997, "yhf": 2002})
>>> d
<pypocketmap[str, int64]: {'b': -10, 'okc': 1997, 'a': 2, 'yhf': 2002}>
>>> [k for k in d], list(d.keys())
(['b', 'okc', 'a', 'yhf'], ['b', 'okc', 'a', 'yhf'])
>>> list(d.values()), list(d.items())
([-10, 1997, 2, 2002], [('b', -10), ('okc', 1997), ('a', 2), ('yhf', 2002)])
>>> len(d)
4
>>> d.clear()
>>> len(d)
0

```

### To-do list

- Add automatic or manual `shrink_to_fit`
- Iterators should be created on the fly and the default iterator should only be used if its refcount is
  zero.
- Iter-next functions should throw an exception if the table has been rehashed, instead of potentially
  skipping elements or yielding them twice.
- `update` should work on any arg when `PyDict_Check` returns true ***or*** `PyMapping_Keys` returns non-null
    - `__or__` and `__ior__` operators can be implemented with this
- The [METH\_FASTCALL](https://docs.python.org/3/c-api/structures.html#c.METH_FASTCALL) convention is
  stable since Python 3.10, and it should be possible to alter the \*Py.c files to use it in place of
  METH\_VARARGS when compiling for 3.10+.

