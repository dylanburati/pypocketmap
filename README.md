# pypocketmap
A high performance python hash table library that consumes significantly less memory than Python
Dictionaries. It currently supports Python 3.5+. It is forked from
[Microdict](https://github.com/touqir14/Microdict) to remove the limitation on the length of string
keys, and add utility methods like `get` and `setdefault`. It also has a slightly different table
design, making use of code ported to C from [abseil-cpp](https://github.com/abseil/abseil-cpp).
__
### Benchmarks
The latest charts are at https://observablehq.com/@dylan-burati-ws/pypocketmap-benchmarks
__
### Installation and Building
You can install Microdict using pip: `pip install pypocketmap`.

Microdict is tested to work on Linux, Mac OSX, and Windows systems. You will need
GCC 7+ on linux/mac osx systems and Visual C++ 14+ compiler on Windows systems to
build the package. For the best performance use on a 64 bit system.

### Usage
The following code snippet shows common uses of the library.

```python
>>> import pypocketmap as pkt

# Generates a dictionary with string keys and signed 64 bit integer values.
>>> d = pkt.create(str, int)  
>>> d = pkt.create(pkt.string, pkt.i64)  # or explicitly

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
<pypocketmap[str, int64]: {"a": 2, "b": -10, "okc": 1997, "yhf": 2002}>
>>> [k for k in d], list(d.keys())
TODO
>>> list(d.values()), list(d.items())
TODO
>>> len(d)
4
>>> d.clear()
>>> len(d)
0

```
__
