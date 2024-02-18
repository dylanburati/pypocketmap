import itertools
import json
import subprocess
import re
import sys

half_configs = [
    {
        "typeTag": "TYPE_TAG_I32",
        "disp": "int32",
        "type": "int32_t",
        "zero": 0,
        "negative_one": -1,
        "from_func": "PyLong_FromLong",
        "as_func": "PyLong_AsLong",
        "format_spec": '"%ld"',
        "short_repr_size": 1,
    },
    {
        "typeTag": "TYPE_TAG_I64",
        "disp": "int64",
        "type": "int64_t",
        "zero": 0,
        "negative_one": -1,
        "from_func": "PyLong_FromLongLong",
        "as_func": "PyLong_AsLongLong",
        "format_spec": '"%lld"',
        "short_repr_size": 1,
    },
    {
        "typeTag": "TYPE_TAG_F32",
        "disp": "float32",
        "type": "float",
        "zero": "0.0f",
        "negative_one": "-1.0f",
        "from_func": "PyFloat_FromDouble",
        "cast_from": "(double) ",
        "as_func": "PyFloat_AsDouble",
        "cast_as": "(float) ",
        "format_spec": '"%g"',
        "short_repr_size": 3,
    },
    {
        "typeTag": "TYPE_TAG_F64",
        "disp": "float64",
        "type": "double",
        "zero": "0.0",
        "negative_one": "-1.0",
        "from_func": "PyFloat_FromDouble",
        "as_func": "PyFloat_AsDouble",
        "format_spec": '"%g"',
        "short_repr_size": 3,
    },
    {
        "typeTag": "TYPE_TAG_STR",
        "disp": "str",
        "type": "char*",
        "zero": "{ .ptr = EMPTY_STR, .len = 0 }",
        "short_repr_size": 2,
    },
]

base_src_path = "pypocketmap"
with open(f"{base_src_path}/str_int64_Py.c", "r", encoding="utf-8") as fp:
    src_lines = [line.rstrip() for line in fp.readlines()]

# base_test_path = "pocketmap/src/test/java/dev/dylanburati/pocketmap"
# with open(
#     f"{base_test_path}/IntPocketMapTest.java", "r", encoding="utf-8"
# ) as fp:
#     test_lines = [line.rstrip() for line in fp.readlines()]

template_rgx = re.compile(r"^([ ]*)/\* template(\([0-9]+\))?! (.*) \*/")
template_all_rgx = re.compile(r"^[ ]*/\* template_all! (.*) \*/")

from_py_normal = r"""
\(.[2]) = \(.[0].cast_as//"")\(.[0].as_func)(\(.[1]));
if (\(.[2]) == \(.[0].negative_one) && PyErr_Occurred()) {
    return \(.[3]);
}
""".strip().replace('\n', r'\n')

from_py_string = r"""
\(if .[4] == "-" then "" else "Py_ssize_t \(.[2])_len;" end)
\(.[2]).ptr = PyUnicode_AsUTF8AndSize(\(.[1]), &\(.[2])_len);
if (\(.[2]).ptr == NULL) {
    return \(.[3]);
}
\(.[2]).len = \(.[2])_len;
""".strip().replace('\n', r'\n')

partial_from_py_normal = r'\n'.join(from_py_normal.split(r'\n')[:2])
partial_from_py_string = r'\n'.join(from_py_string.split(r'\n')[1:3])

repr_write_normal = r"""
size_t \(.[1])_len = snprintf(\(.[1])_repr, 47, \(.[0].format_spec), \(.[1]));
if (_PyUnicodeWriter_WriteASCIIString(&writer, \(.[1])_repr, \(.[1])_len) < 0) {
    _PyUnicodeWriter_Dealloc(&writer);
    return NULL;
}
""".strip().replace('\n', r'\n')

repr_write_string = r"""
\(.[1])_obj = PyUnicode_FromStringAndSize(\(.[1]).ptr, \(.[1]).len);
if (\(.[1])_obj == NULL) {
    _PyUnicodeWriter_Dealloc(&writer);
    return NULL;
}
\(.[1])_repr = PyObject_Repr(\(.[1])_obj);
if (\(.[1])_repr == NULL) {
    _PyUnicodeWriter_Dealloc(&writer);
    Py_CLEAR(\(.[1])_obj);
    return NULL;
}
if (_PyUnicodeWriter_WriteStr(&writer, \(.[1])_repr) < 0) {
    _PyUnicodeWriter_Dealloc(&writer);
    Py_CLEAR(\(.[1])_obj);
    return NULL;
}
Py_CLEAR(\(.[1])_obj);
"""

def fill_templates(configs, lines):
    result = [[] for _ in configs]
    i = 0
    config_b = b"".join((json.dumps(c).encode("utf-8") + b"\n") for c in configs)
    unconditional_replaces = [{} for _ in configs]
    while i < len(lines):
        template_all_match = template_all_rgx.match(lines[i])
        if template_all_match:
            srcs = json.loads(template_all_match.group(1))
            for j in range(len(result)):
                if dests := configs[j].get("example_values"):
                    for src, dst in zip(srcs, itertools.cycle(dests)):
                        unconditional_replaces[j][str(src)] = dst
                if configs[j].get("keep"):
                    result[j].append(lines[i])
            i += 1
            continue
        to_add = [[] for _ in configs]
        template_match = template_rgx.match(lines[i])
        if not template_match:
            for j in range(len(result)):
                to_add[j].append(lines[i])
            i += 1
        else:
            replace_count = 1
            indent = template_match.group(1)
            if count_arg := template_match.group(2):
                replace_count = int(count_arg[1:-1])
            jq_script = (
                r'def to_py: if .[0].type == "char*" then '
                r' "PyUnicode_DecodeUTF8(\(.[1]).ptr, \(.[1]).len, NULL)"'
                r' else "\(.[0].from_func)(\(.[0].cast_from//"")\(.[1]))" end;'
                '\n'
            )
            jq_script += (
                r'def from_py: if .[0].type == "char*" then "{}"'.format(from_py_string) +
                r' else "{}" end;'.format(from_py_normal) +
                '\n'
            )
            jq_script += (
                r'def partial_from_py: if .[0].type == "char*" then "{}"'.format(partial_from_py_string) +
                r' else "{}" end;'.format(partial_from_py_normal) +
                '\n'
            )
            jq_script += (
                r'def key_error: if .[0].type == "char*" then '
                r' "PyErr_SetString(PyExc_KeyError, \(.[1]).ptr)"'
                r' else "char msg[48];\nsnprintf(msg, 47, \(.[0].format_spec), \(.[1]));\nPyErr_SetString(PyExc_KeyError, msg);" end;'
                '\n'
            )
            jq_script += (
                r'def repr_declare: if .[0].type == "char*" then '
                r' "PyObject* \(.[1])_obj = NULL;\nPyObject* \(.[1])_repr;"'
                r' else "char \(.[1])_repr[48];" end;'
                '\n'
            )
            jq_script += (
                r'def repr_write: if .[0].type == "char*" then "{}"'.format(repr_write_string) +
                r' else "{}" end;'.format(repr_write_normal) +
                '\n'
            )
            # TODO
            jq_script += fr'"{template_match.group(3)}"'
            proc = subprocess.Popen(
                ["jq", "-c", jq_script],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            out_bytes, err_bytes = proc.communicate(config_b + b"\n")
            if err_bytes:
                print(f"template error on line {i+1}", file=sys.stderr)
                sys.stderr.buffer.write(err_bytes)
                break
            for j, render in enumerate(map(json.loads, map(bytes.rstrip, out_bytes.splitlines()))):
                to_add[j].extend(
                    (indent + e) for e in render.split("\n") if e != ""
                )
            for j in range(len(result)):
                if configs[j].get("keep"):
                    result[j].append(lines[i])
            i += 1 + replace_count

        for j in range(len(to_add)):
            for line in to_add[j]:
                replace_indices = []
                for src, dst in unconditional_replaces[j].items():
                    line_idx = 0
                    while line_idx < len(line):
                        nxt = line.find(src, line_idx)
                        if nxt == -1:
                            break
                        replace_indices.append((nxt, nxt + len(src), dst))
                        line_idx = nxt + len(src)
                replace_indices.sort(key=lambda e: -e[0])
                for start, end, dst in replace_indices:
                    line = line[:start] + dst + line[end:]
                result[j].append(line)
    return result


src_configs = [{"key": c1, "val": c2} for c1 in half_configs[-1:] for c2 in half_configs]
first_config = None
for c in src_configs:
    if c["key"]["disp"] == "str" and c["val"]["disp"] == "int64":
        first_config = c
        break
else:
    raise ValueError()
first_config["keep"] = True
src_configs = [c for c in src_configs if c != first_config]
src_sanity, *src_outs = fill_templates([first_config, *src_configs], src_lines)
if src_sanity != src_lines:
    import pdb

    pdb.set_trace()
    sys.exit(1)
# test_sanity, *test_outs = fill_templates([int_config, *configs, *test_only_configs], test_lines)
# if test_sanity != test_lines:
#     import pdb
#
#     pdb.set_trace()
#     sys.exit(1)

for lst, c in zip(src_outs, src_configs):
    src_file = f"{c['key']['disp']}_{c['val']['disp']}_Py.c"
    with open(f"{base_src_path}/{src_file}", "w", encoding="utf-8") as fp:
        fp.write("\n".join(lst))
        fp.write("\n")
# for lst, c in zip(test_outs, configs + test_only_configs):
#     test_file = f"{c['val']['disp']}PocketMapTest.java"
#     with open(f"{base_test_path}/{test_file}", "w", encoding="utf-8") as fp:
#         fp.write("\n".join(lst))
#         fp.write("\n")
