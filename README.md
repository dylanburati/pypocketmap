# pypocketmap

NOTE: this package is in beta. The current repo contains implementations
for these key/value combinations: `[str, _]` and `[i64, i64]`.

A high performance python hash table library that consumes significantly less
memory than Python Dictionaries. It currently supports Python 3.6+. It is forked from
[Microdict](https://github.com/touqir14/Microdict) to remove the limitation on the
length of string keys, and add utility methods like `get` and `setdefault`.
It also has a slightly different table design, making use of code ported to
C from [abseil-cpp](https://github.com/abseil/abseil-cpp).

### Benchmarks
The latest charts are at https://observablehq.com/@dylan-burati-ws/pypocketmap-benchmarks

### Installation and Building
You can install using pip: `pip install pypocketmap`.

pypocketmap is tested to work on Linux, Mac OSX, and Windows systems. You will need
GCC 7+ on linux/mac osx systems and Visual C++ 14+ compiler on Windows systems to
build the package. For the best performance use on a 64 bit system.

### Usage
The following code snippet shows common uses of the library.

```python
>>> import pypocketmap as pkm

# Generates a dictionary with string keys and signed 64 bit integer values.
>>> d = pkm.create(str, int)
>>> d = pkm.create(pkm.string_, pkm.int64_)  # or explicitly

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

### How it works

#### The C parts

The map part of pypocketmap is just a C port of abseil-cpp's SwissTable, which is explained nicely in this blog post series: https://www.kylematsuda.com/blog/writing_a_hashmap_part_2.

The memory savings (pocket part) are partially due to an optimization for small strings - when stored in
the key or value array of the map, a string is a 16-byte `packed_str_t`. This is a union:

- contained: the data fits in 15 bytes, and the length and sentinel bit `1` fit in 1 byte.
- spilled: the struct holds a `char *` pointing to the data, and the length and sentinel bit `0` take up
  the remaining 8 bytes.
  - 4 bytes are padding on 32-bit platforms; 8 bytes end up unused because the longer lengths would fail
    to allocate.

The C++ standard library and Rust crate `byteyarn` do something similar - I learned about this from
the crate author's blog post: https://mcyoung.xyz/2023/08/09/yarns/.

#### The Python C API parts

The file [str\_int64\_Py.c](./pypocketmap/str_int64_Py.c) is the only Python module definition which should
be edited. That file and [abstract.h](./pypocketmap/abstract.h) use C macros and `typedef` to fake generics.
Many Python-specific blocks in the former file are also annotated with custom `template!` comments, which
I came up with for this library's Java equivalent, [pocketmap](https://github.com/dylanburati/pocketmap).

The `template!` comments are the reason for the nearly equivalent `*_Py.c` files in the repo. Essentially,
the macros and comments implement the following traits for the key and value types. Since the
"implementations" are just inlined into the files, this is prone to errors. It is probably doable in C++,
but for now I'm choosing to add tests rather than convert the C code.

```rust
trait Value {
  type Packed;

  // the `&` operator indicates something borrowed.
  // physically, both Self and &Self can be a struct { char* data, uint64_t len }
  // semantically, &Self means that the data field is readonly, and it's not safe
  // to use the pointer once the source of the borrow is possibly freed/dropped.
  // ===

  /// Converts the possibly-borrowed value to an owned value independent of any
  /// PyObjects and stores it.
  fn p_set(arr: *mut Self::Packed, index: usize, elem: &Self);

  /// Frees anything alloc'd in a previous `p_set(arr, index)`.
  /// No-op for everything except spilled strings.
  fn p_unset(arr: *mut Self::Packed, index: usize);

  /// Borrows the value - the caller is responsible for making sure `arr[index]` is
  /// set, and stays set while the returned &Self is in use.
  fn p_get(arr: *Self::Packed, index: usize) -> &Self;

  /// Converts the Python object to a C value.
  fn from_py(obj: &PyObject) -> Result<&Self, ()>;

  /// Convert the C value to a new Python object which owns its data. The caller
  /// owns the returned Python object, and should eventually either decrement its
  /// reference count, or return it as the result of a pymethodfunc.
  fn as_py(&self) -> PyObject;

  /// For `repr`.
  fn fmt(&self, f: &mut _PyUnicodeWriter) -> Result<(), ()>;

  /// For `setdefault` when the value arg is left out.
  fn default() -> &'static Self;

  /// For `__eq__` when acting as the value type. For pretty much all methods
  /// when acting as the key type.
  fn eq(&self, other: &Self) -> bool;
}

trait Key: Value + Hash {}
```

### Not yet supported

- Automatic or manual `shrink_to_fit`
- Concurrent modification exceptions if an iterator is used after the table has been rehashed. The
  implementation can skip elements or yield them twice if used incorrectly.
- `update` should work on any arg when `PyDict_Check` returns true ***or*** `PyMapping_Keys` returns non-null
    - `__or__` and `__ior__` operators can be implemented with this
- The [METH\_FASTCALL](https://docs.python.org/3/c-api/structures.html#c.METH_FASTCALL) convention is
  stable since Python 3.10, and it should be possible to alter the \*Py.c files to use it in place of
  METH\_VARARGS when compiling for 3.10+.
- Additional overflow checking when `LLONG_MAX != INT64_MAX` or (more likely) `LONG_MAX != INT32_MAX`

