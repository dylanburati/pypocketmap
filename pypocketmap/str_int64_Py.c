#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "flags.h"
#define KEY_TYPE_TAG TYPE_TAG_STR
#define VAL_TYPE_TAG TYPE_TAG_I64
#include "abstract.h"

typedef struct {
    PyObject_HEAD
    h_t* ht;    
    uint32_t iter_idx;
} iterObj;

typedef struct {
    PyObject_HEAD
    h_t* ht;
    bool valid_ht;
    iterObj* key_iterator;
    iterObj* value_iterator;
    iterObj* item_iterator;
} dictObj;

static PyObject* key_iter(iterObj* self);
static PyObject* key_iternext(iterObj* self);
static PyObject* value_iter(iterObj* self);
static PyObject* value_iternext(iterObj* self);
static PyObject* item_iter(iterObj* self);
static PyObject* item_iternext(iterObj* self);

static PyTypeObject keyIterType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap_keys[str, int64]",
    .tp_doc = "",
    .tp_basicsize = sizeof(iterObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyType_GenericNew,
    .tp_iter = (getiterfunc) key_iter,
    .tp_iternext = (iternextfunc) key_iternext,
};

static PyTypeObject valueIterType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap_values[str, int64]",
    .tp_doc = "",
    .tp_basicsize = sizeof(iterObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyType_GenericNew,
    .tp_iter = (getiterfunc) value_iter,
    .tp_iternext = (iternextfunc) value_iternext,
};

static PyTypeObject itemIterType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap_items[str, int64]",
    .tp_doc = "",
    .tp_basicsize = sizeof(iterObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyType_GenericNew,
    .tp_iter = (getiterfunc) item_iter,
    .tp_iternext = (iternextfunc) item_iternext,
};

/**
 * Returns an iterator for iterating over the dictionary keys. This function is invoked when dict.keys() method is called.
 */
static PyObject* key_iter(iterObj* self) {
    Py_INCREF(self);
    self->iter_idx = 0;
    return (PyObject *) self;    
}

/**
 * Iterates over the keyss when __next__ is called on the iterator. Each time this function is called by __next__, the next keys is returned.
 */
static PyObject* key_iternext(iterObj* self) {
    h_t* h = self->ht;
    for (uint32_t i = self->iter_idx; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            k_t key = _get_key(h->keys, i);
            self->iter_idx = i+1;
            return PyUnicode_DecodeUTF8(key.ptr, key.len, NULL);
        }
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

/**
 * Returns an iterator for iterating over the dictionary values. This function is invoked when dict.values() method is called.
 */
static PyObject* value_iter(iterObj* self) {
    Py_INCREF(self);
    self->iter_idx = 0;
    return (PyObject *) self;    
}

/**
 * Iterates over the values when __next__ is called on the iterator. Each time this function is called by __next__, the next value is returned.
 */
static PyObject* value_iternext(iterObj* self) {
    h_t* h = self->ht;
    for (uint32_t i = self->iter_idx; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            self->iter_idx = i+1;
            return PyLong_FromLongLong(h->vals[i]);
        }
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

/**
 * Returns an iterator for iterating over the dictionary items. This function is invoked when dict.items() method is called.
 */
static PyObject* item_iter(iterObj* self) {
    Py_INCREF(self);
    self->iter_idx = 0;
    return (PyObject *) self;    
}


/**
 * Iterates over the items when __next__ is called on the iterator. Each time this function is called by __next__, the next item (key, value) is returned.
 */
static PyObject* item_iternext(iterObj* self) {
    h_t* h = self->ht;
    for (uint32_t i = self->iter_idx; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            k_t key = _get_key(h->keys, i);
            self->iter_idx = i+1;
            return PyTuple_Pack(2, PyUnicode_DecodeUTF8(key.ptr, key.len, NULL), PyLong_FromLongLong(h->vals[i]));
        }
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

/*
Called by the destructor for deleting the hashtable.
*/
void _destroy(dictObj* self) {
    if (self->valid_ht) {
        mdict_destroy(self->ht);
        self->valid_ht = false;
    }
}

/**
 * Called by the constructor for allocating and initializing the hashtable.
 */
void _create(dictObj* self, uint32_t num_buckets){
    if (!self->valid_ht) {
        self->ht = mdict_create(num_buckets, true);  
        self->valid_ht = true;
    }
}
 
/**
 * The destructor
 */
static void custom_dealloc(dictObj* self) {
    _destroy(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/**
 * Allocates the dictObj
 */
static PyObject* custom_new(PyTypeObject *type, PyObject *args) {
    dictObj* self = (dictObj*) type->tp_alloc(type, 0);
    self->ht = NULL;
    self->valid_ht = false;
    return (PyObject*) self;
}

/**
 * Constructor for allocating and initializing the hashtable along with the iterators.
 */
static int custom_init(dictObj* self, PyObject *args) {
    unsigned int num_buckets = 32;

    if (!PyArg_ParseTuple(args, "|I", &num_buckets)) {
        Py_DECREF(self);
        return -1;
    }

    _create(self, num_buckets);

    self->key_iterator = (iterObj *) keyIterType_str_int64.tp_alloc(&keyIterType_str_int64, 0);
    self->key_iterator->ht = self->ht;
    self->key_iterator->iter_idx = 0;

    self->value_iterator = (iterObj *) valueIterType_str_int64.tp_alloc(&valueIterType_str_int64, 0); 
    self->value_iterator->ht = self->ht;
    self->value_iterator->iter_idx = 0;

    self->item_iterator = (iterObj *) itemIterType_str_int64.tp_alloc(&itemIterType_str_int64, 0); 
    self->item_iterator->ht = self->ht;
    self->item_iterator->iter_idx = 0;

    return 0;
}

/**
 * This function is invoked when dict.get(k, [default]) is called.
 */
static PyObject* get(dictObj* self, PyObject* args) {
    PyObject* key_obj;
    PyObject* default_obj = NULL;

    if (!PyArg_ParseTuple(args, "O|O", &key_obj, &default_obj)) {
        return NULL;
    }
    k_t key;
    Py_ssize_t len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
    if (key.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return NULL;
    }
    key.len = len;

    v_t val;
    if (!mdict_get(self->ht, key, &val)) {
        if (default_obj != NULL) {
            Py_INCREF(default_obj);
            return default_obj;
        }
        return Py_BuildValue("");
    }
    return PyLong_FromLongLong(val);
}

/**
 * dict.pop() invokes this function. If provided, a default is returned when the key is not found;
 * otherwise a KeyError is raised.
 */
static PyObject* pop(dictObj* self, PyObject* args) {
    PyObject* key_obj;
    PyObject* default_obj = NULL;

    if (!PyArg_ParseTuple(args, "O|O", &key_obj, &default_obj)) {
        return NULL;
    }

    k_t key;
    Py_ssize_t len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
    if (key.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return NULL;
    }
    key.len = len;

    v_t v;
    if (!mdict_remove(self->ht, key, &v)) {
        if (default_obj != NULL) {
            Py_INCREF(default_obj);
            return default_obj;
        }
        PyErr_SetString(PyExc_KeyError, key.ptr);
        return NULL;
    }
    return PyLong_FromLongLong(v);
}

/**
 * This is invoked for the python expression d.setdefault(key, [default]). If no default is passed, zero is used.
 */
static PyObject* setdefault(dictObj* self, PyObject* args) {
    PyObject* key_obj;
    v_t dfault = 0;

    if (!PyArg_ParseTuple(args, "O|L", &key_obj, &dfault)) {
        return NULL;
    }

    k_t key;
    Py_ssize_t len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
    if (key.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return NULL;
    }
    key.len = len;
 
    // can use &dfault as val_box because the default doesn't need to be read
    mdict_set(self->ht, key, dfault, &dfault, false);
    if (self->ht->error_code) {
        PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
        return NULL;
    }
    return PyLong_FromLongLong(dfault);
}

/**
 * dict.clear() invokes this function.
 */
static PyObject* clear(dictObj* self) {
    mdict_clear(self->ht);
    self->value_iterator->ht = self->ht;
    self->value_iterator->iter_idx = 0;
    self->item_iterator->ht = self->ht;
    self->item_iterator->iter_idx = 0;
    return Py_BuildValue("");
}

/**
 * This function updates the hashtable with items from a given Python Dictionary. In case the python
 * dictionary contains an item with non-matching types, then a TypeError will be raised.
 */
int _update_from_Pydict(dictObj* self, PyObject* dict) {
    PyObject* key_obj;
    PyObject* value_obj;
    Py_ssize_t pos = 0;
    Py_ssize_t len;
    k_t key;
    v_t val;
    while (PyDict_Next(dict, &pos, &key_obj, &value_obj)) {
        val = (v_t) PyLong_AsLongLong(value_obj);
        if (val == -1 && PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "Values must be integers in the range [-2**63, 2**63)");
            return -1;
        }

        key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
        if (key.ptr == NULL) {
            PyErr_SetString(PyExc_TypeError, "Key must be a string");
            return NULL;
        }
        key.len = len;

        mdict_set(self->ht, key, val, NULL, true);
        if (self->ht->error_code) {
            PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
            return -1;
        }
    }

    return 0;   
}

/**
 * This function updates the hashtable with all the items from another dictionary (dict) of the same key, value type.
 */
int _update_from_mdict(dictObj* self, dictObj* dict) {
    h_t* h = self->ht;
    h_t* other = dict->ht;

    for (uint32_t i = 0; i < other->num_buckets; i++) {
        if (_bucket_is_live(other->flags, i)) {
            mdict_set(h, _get_key(other->keys, i), other->vals[i], NULL, true);
            if (self->ht->error_code) {
                PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
                return -1;
            }
        }
    }
    return 0;
}

/**
 * This function is called for the python expression 'k in dict'. k must be of the same type as the hashtable keys.
 */
static int _contains_(dictObj* self, PyObject* key_obj) {
    k_t key;
    Py_ssize_t len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
    if (key.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return -1;
    }
    key.len = len;

    return mdict_contains(self->ht, key);
}

/**
 * This function is called when len(dict) is called. It returns the total number of items present.
 */
static int _len_(dictObj* self) {
    return self->ht->size;
}


/**
 * This function is invoked when dict[k] is called.
 */
static PyObject* _getitem_(dictObj* self, PyObject* key_obj){
    k_t key;
    Py_ssize_t len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
    if (key.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return NULL;
    }
    key.len = len;

    v_t val;
    if (!mdict_get(self->ht, key, &val)) {
        PyErr_SetString(PyExc_KeyError, key.ptr);
        return NULL;
    }
    return PyLong_FromLongLong(val);
}

/**
 * This is invoked for the python expression d[key] = value. Both key and value must be of the hashtable type.
 * This is also invoke for del d[key], in which case the `val_obj` is NULL
 */
static int _setitem_(dictObj* self, PyObject* key_obj, PyObject* val_obj) {
    k_t key;
    Py_ssize_t len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &len);
    if (key.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "Key must be a string");
        return NULL;
    }
    key.len = len;
    if (val_obj == NULL) {
        if (!mdict_remove(self->ht, key, NULL)) {
            PyErr_SetString(PyExc_KeyError, key.ptr);
            return NULL;
        }
        return 0;
    }

    v_t val = (v_t) PyLong_AsLongLong(val_obj);
    if (val == -1 && PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Values must be integers in the range [-2**63, 2**63)");
        return -1;
    }

    mdict_set(self->ht, key, val, NULL, true);
    if (self->ht->error_code) {
        PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
        return -1;
    }
    return 0;
}

/**
 * This is invoked for the python expression d BINOP other (BINOP is ==, !=, <, >, <=, or >=).
 */
static PyObject* _richcmp_(dictObj* self, PyObject* other, int op) {
    if (op != Py_EQ && op != Py_NE) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }
    if (!PyMapping_Check(other)) {
        return PyBool_FromLong(op != Py_EQ);
    }
    if (PyMapping_Size(other) != self->ht->size) {
        return PyBool_FromLong(op != Py_EQ);
    }

    bool is_equal = true;
    h_t* h = self->ht;
    for (uint32_t i = 0; is_equal && i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            k_t key = _get_key(h->keys, i);
            PyObject* other_val_obj = PyMapping_GetItemString(other, key.ptr);
            if (other_val_obj == NULL) {
                is_equal = false;
                break;
            }
            v_t other_val = PyLong_AsLongLong(other_val_obj);
            if (other_val == -1 && PyErr_Occurred()) {
                PyErr_Clear();
                is_equal = false;
                break;
            }
            is_equal = (h->vals[i] == other_val);
        }
    }
    return PyBool_FromLong((op == Py_EQ) == is_equal);
}

/**
 * Formats the map as a string
 */
static PyObject* _repr_(dictObj* self) {
    h_t* h = self->ht;
    if (h->size == 0) {
        return PyUnicode_FromString("<pypocketmap[str, int64]: {}>");
    }

    _PyUnicodeWriter writer;
    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;
    /* "<pypocketmap[str, int64]: {" + "'': 2" + ", '_': 4" * (len - 1) + "}>" */
    writer.min_length = 1 + 23 + 3 + 5 + (2 + 6) * (h->size - 1) + 2;

    if (_PyUnicodeWriter_WriteASCIIString(&writer, "<pypocketmap[str, int64]: {", 1 + 23 + 3) < 0) {
        _PyUnicodeWriter_Dealloc(&writer);
        return NULL;
    }
    k_t key;
    PyObject* key_obj = NULL;
    PyObject* key_repr;
    char value_repr[21];  // long enough for -9223372036854775808 with null terminator
    bool first = true;
    for (uint32_t i = 0; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            if (!first) {
                if (_PyUnicodeWriter_WriteASCIIString(&writer, ", ", 2) < 0) {
                    _PyUnicodeWriter_Dealloc(&writer);
                    return NULL;
                }
            }
            first = false;
            key = _get_key(h->keys, i);
            key_obj = PyUnicode_FromStringAndSize(key.ptr, key.len);
            if (key_obj == NULL) {
                _PyUnicodeWriter_Dealloc(&writer);
                return NULL;
            }
            key_repr = PyObject_Repr(key_obj);
            if (key_repr == NULL) {
                _PyUnicodeWriter_Dealloc(&writer);
                Py_CLEAR(key_obj);
                return NULL;
            }
            if (_PyUnicodeWriter_WriteStr(&writer, key_repr) < 0) {
                _PyUnicodeWriter_Dealloc(&writer);
                Py_CLEAR(key_obj);
                return NULL;
            }
            Py_CLEAR(key_obj);

            if (_PyUnicodeWriter_WriteASCIIString(&writer, ": ", 2) < 0) {
                _PyUnicodeWriter_Dealloc(&writer);
                return NULL;
            }
            size_t value_len = snprintf(value_repr, 20, "%lld", h->vals[i]);
            if (_PyUnicodeWriter_WriteASCIIString(&writer, value_repr, value_len) < 0) {
                _PyUnicodeWriter_Dealloc(&writer);
                return NULL;
            }
        }
    }
    if (_PyUnicodeWriter_WriteASCIIString(&writer, "}>", 2) < 0) {
        _PyUnicodeWriter_Dealloc(&writer);
        return NULL;
    }

    return _PyUnicodeWriter_Finish(&writer);
}

/**
 * Returns an iterator for keys when __iter__(dict) is called 
 */
static PyObject* keys(dictObj* self) {
    self->key_iterator->iter_idx = 0;
    Py_INCREF((PyObject*) self->key_iterator);
    return (PyObject*) self->key_iterator;
}

/**
 * Returns the value iterator
 */
static PyObject* values(dictObj* self) {
    Py_INCREF((PyObject*) self->value_iterator);
    return (PyObject*) self->value_iterator;
}

/**
 * Returns the item iterator
 */
static PyObject* items(dictObj* self) {
    Py_INCREF((PyObject*) self->item_iterator);
    return (PyObject*) self->item_iterator;    
}

/**
 * Returns a new pypocketmap containing all items present in this hashtable when dict.copy() is called.
 */
static PyObject* copy(dictObj* self) {
    PyObject* args = Py_BuildValue("I", self->ht->num_buckets);
    dictObj* new_obj = (dictObj *) PyObject_CallObject((PyObject *)((PyObject *) self)->ob_type, args);
    Py_DECREF(args);
    _update_from_mdict(new_obj, self);
    return (PyObject*) new_obj;
}

static PyObject* update(dictObj* self, PyObject* args);

static PyMethodDef methods_str_str[] = {
    {"get", (PyCFunction)get, METH_VARARGS, "Return the value for `key` if `key` is in the dictionary, else `default`. If `default` is not given, it defaults to None, so that this method never raises a KeyError."},
    {"pop", (PyCFunction)pop, METH_VARARGS, "If key is in the dictionary, remove it and return its value, else return `default`. If `default` is not given and `key` is not in the dictionary, a KeyError is raised."},
    {"setdefault", (PyCFunction)setdefault, METH_VARARGS, "If `key` is in the dictionary, return its value. If not, insert `key` with a value of `default` and return `default`. default defaults to 0."},
    {"clear", (PyCFunction)clear, METH_VARARGS, "Remove all items from the dictionary."},
    {"update", (PyCFunction)update, METH_VARARGS, "Updates the map with all key-value pairs within the given input."},
    {"keys", (PyCFunction)keys, METH_VARARGS, "Returns an iterator over the map's keys"},
    {"values", (PyCFunction)values, METH_VARARGS, "Returns an iterator over the map's values"},
    {"items", (PyCFunction)items, METH_VARARGS, "Returns an iterator over the map's (key, value) pairs"},
    {"copy", (PyCFunction)copy, METH_VARARGS, "Returns a deep copy of the hashtable"},
    {NULL, NULL, 0, NULL}
};

static PySequenceMethods sequence_str_int64 = {
    (lenfunc) _len_,                    /* sq_length */
    0,                                  /* sq_concat */
    0,                                  /* sq_repeat */
    0,                                  /* sq_item */
    0,                                  /* sq_slice */
    0,                                  /* sq_ass_item */
    0,                                  /* sq_ass_slice */
    (objobjproc) _contains_,            /* sq_contains */
};

static PyMappingMethods mapping_str_int64 = {
    (lenfunc) _len_, /*mp_length*/
    (binaryfunc)_getitem_, /*mp_subscript*/
    (objobjargproc)_setitem_, /*mp_ass_subscript*/
};

static PyTypeObject dictType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap[str, int64]",
    .tp_doc = "pypocketmap[str, int64]", 
    .tp_basicsize = sizeof(dictObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = (newfunc) custom_new,
    .tp_init = (initproc) custom_init,
    .tp_dealloc = (destructor) custom_dealloc,
    .tp_methods = methods_str_str,
    .tp_as_sequence = &sequence_str_int64,
    .tp_as_mapping = &mapping_str_int64,
    .tp_iter = (getiterfunc) keys,
    .tp_iternext = (iternextfunc) key_iternext,
    .tp_richcompare = (richcmpfunc) _richcmp_,
    .tp_repr = (reprfunc) _repr_,
};

/**
 * Invoked when dict.update() is called. It takes an argument which must be either a Python dictionary or a
 * pypocketmap of the same type. It adds all the items from the argument dictionary given to its hashtable. See _update_from_Pydict and
 * _update_from_mdict for further documentation.
 *
 * TODO try to get this working for generic mappings
 */
static PyObject* update(dictObj* self, PyObject* args) {
    PyObject* other;
    bool is_pydict = PyArg_ParseTuple(args, "O!", &PyDict_Type, &other);

    if (!is_pydict) {
        PyErr_Clear();
        if (!PyArg_ParseTuple(args, "O", &other)) {
            return NULL;
        }

        if (PyObject_IsInstance(other, (PyObject *) &dictType_str_int64) != 1) {
            PyErr_SetString(PyExc_TypeError, "Argument needs to be either a pypocketmap[str, int64] or compatible Python dictionary");
            return NULL;
        }
    }

    if (is_pydict) {
        if (_update_from_Pydict(self, other) == -1) {
            return NULL;
        }
    } else {
        dictObj* dict = (dictObj*) other;
        if (_update_from_mdict(self, dict) == -1) {
            return NULL;
        }
    }

    return Py_BuildValue("");
}

static struct PyModuleDef moduleDef_str_int64 = {
    PyModuleDef_HEAD_INIT,
    "str_int64", /* name of module */
    "pypocketmap[str, int64]", // Documentation of the module
    -1,   /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
};

PyMODINIT_FUNC PyInit_str_int64(void) {
    PyObject* obj;

    if (PyType_Ready(&dictType_str_int64) < 0)
        return NULL;

    if (PyType_Ready(&keyIterType_str_int64) < 0)
        return NULL;

    if (PyType_Ready(&valueIterType_str_int64) < 0)
        return NULL;

    if (PyType_Ready(&itemIterType_str_int64) < 0)
        return NULL;

    obj = PyModule_Create(&moduleDef_str_int64);
    if (obj == NULL)
        return NULL;

    Py_INCREF(&dictType_str_int64);
    if (PyModule_AddObject(obj, "create", (PyObject *) &dictType_str_int64) < 0) {
        Py_DECREF(&dictType_str_int64);
        Py_DECREF(obj);
        return NULL;
    }

    return obj;
}
