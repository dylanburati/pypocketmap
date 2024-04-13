#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "flags.h"
/* template(2)! #define KEY_TYPE_TAG \(.key.typeTag)\n#define VAL_TYPE_TAG \(.val.typeTag) */
#define KEY_TYPE_TAG TYPE_TAG_STR
#define VAL_TYPE_TAG TYPE_TAG_I64
#include "abstract.h"

typedef struct {
    PyObject_HEAD
    h_t* ht;
    bool valid_ht;
} dictObj;

typedef struct {
    PyObject_HEAD
    dictObj* owner;
    uint32_t iter_idx;
} iterObj;

static void iter_dealloc(iterObj* self);
static int iter_traverse(iterObj* self, visitproc visit, void* arg);
static PyObject* key_iternext(iterObj* self);
static PyObject* value_iternext(iterObj* self);
static PyObject* item_iternext(iterObj* self);

/* template(3)! static PyTypeObject keyIterType_\(.key.disp)_\(.val.disp) = {\n    PyVarObject_HEAD_INIT(NULL, 0)\n    .tp_name = \"pypocketmap_keys[\(.key.disp), \(.val.disp)]\", */
static PyTypeObject keyIterType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap_keys[str, int64]",
    .tp_doc = "",
    .tp_basicsize = sizeof(iterObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_dealloc = (destructor) iter_dealloc,
    .tp_traverse = (traverseproc) iter_traverse,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc) key_iternext,
};

/* template(3)! static PyTypeObject valueIterType_\(.key.disp)_\(.val.disp) = {\n    PyVarObject_HEAD_INIT(NULL, 0)\n    .tp_name = \"pypocketmap_values[\(.key.disp), \(.val.disp)]\", */
static PyTypeObject valueIterType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap_values[str, int64]",
    .tp_doc = "",
    .tp_basicsize = sizeof(iterObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_dealloc = (destructor) iter_dealloc,
    .tp_traverse = (traverseproc) iter_traverse,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc) value_iternext,
};

/* template(3)! static PyTypeObject itemIterType_\(.key.disp)_\(.val.disp) = {\n    PyVarObject_HEAD_INIT(NULL, 0)\n    .tp_name = \"pypocketmap_items[\(.key.disp), \(.val.disp)]\", */
static PyTypeObject itemIterType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pypocketmap_items[str, int64]",
    .tp_doc = "",
    .tp_basicsize = sizeof(iterObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_dealloc = (destructor) iter_dealloc,
    .tp_traverse = (traverseproc) iter_traverse,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc) item_iternext,
};

static PyObject* iter_new(dictObj* owner, PyTypeObject* itertype) {
    iterObj* iterator = PyObject_GC_New(iterObj, itertype);
    if (iterator == NULL) {
        return NULL;
    }
    Py_INCREF(owner);
    iterator->owner = owner;
    iterator->iter_idx = 0;
    PyObject_GC_Track(iterator);
    return (PyObject *)iterator;
}

static void iter_dealloc(iterObj* self) {
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->owner);
    PyObject_GC_Del(self);
}

static int iter_traverse(iterObj* self, visitproc visit, void* arg) {
    Py_VISIT(self->owner);
    return 0;
}

/**
 * Iterates over the keyss when __next__ is called on the iterator. Each time this function is called by __next__, the next keys is returned.
 */
static PyObject* key_iternext(iterObj* self) {
    if (self->owner == NULL) {
        return NULL;
    }
    h_t* h = self->owner->ht;
    for (uint32_t i = self->iter_idx; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            k_t key = KEY_GET(h->keys, i);
            self->iter_idx = i+1;
            /* template! return \([.key, "key"] | to_py); */
            return PyUnicode_DecodeUTF8(key.ptr, key.len, NULL);
        }
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

/**
 * Iterates over the values when __next__ is called on the iterator. Each time this function is called by __next__, the next value is returned.
 */
static PyObject* value_iternext(iterObj* self) {
    if (self->owner == NULL) {
        return NULL;
    }
    h_t* h = self->owner->ht;
    for (uint32_t i = self->iter_idx; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            v_t val = VAL_GET(h->vals, i);
            self->iter_idx = i+1;
            /* template! return \([.val, "val"] | to_py); */
            return PyLong_FromLongLong(val);
        }
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

/**
 * Iterates over the items when __next__ is called on the iterator. Each time this function is called by __next__, the next item (key, value) is returned.
 */
static PyObject* item_iternext(iterObj* self) {
    if (self->owner == NULL) {
        return NULL;
    }
    h_t* h = self->owner->ht;
    for (uint32_t i = self->iter_idx; i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            k_t key = KEY_GET(h->keys, i);
            v_t val = VAL_GET(h->vals, i);
            self->iter_idx = i+1;
            /* template! PyObject* key_obj = \([.key, "key"] | to_py); */
            PyObject* key_obj = PyUnicode_DecodeUTF8(key.ptr, key.len, NULL);
            /* template! PyObject* val_obj = \([.val, "val"] | to_py); */
            PyObject* val_obj = PyLong_FromLongLong(val);
            PyObject* item_obj = PyTuple_Pack(2, key_obj, val_obj);
            // tuple should have the only reference to the key and value objects
            Py_DECREF(key_obj);
            Py_DECREF(val_obj);
            return item_obj;
        }
    }
    PyErr_SetNone(PyExc_StopIteration);
    return NULL;
}

/**
 * Called by the destructor for deleting the hashtable.
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
    /* template(6)! \([.key, "key_obj", "key", "NULL", "key_len"] | from_py) */
    Py_ssize_t key_len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
    if (key.ptr == NULL) {
        return NULL;
    }
    key.len = key_len;

    v_t val;
    if (!mdict_get(self->ht, key, &val)) {
        if (default_obj != NULL) {
            Py_INCREF(default_obj);
            return default_obj;
        }
        return Py_BuildValue("");
    }
    /* template! return \([.val, "val"] | to_py); */
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
    /* template(6)! \([.key, "key_obj", "key", "NULL", "key_len"] | from_py) */
    Py_ssize_t key_len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
    if (key.ptr == NULL) {
        return NULL;
    }
    key.len = key_len;

    uint32_t idx;
    if (!mdict_prepare_remove(self->ht, key, &idx)) {
        if (default_obj != NULL) {
            Py_INCREF(default_obj);
            return default_obj;
        }
        /* template! \([.key, "key"] | key_error); */
        PyErr_SetString(PyExc_KeyError, key.ptr);
        return NULL;
    }
    v_t val = VAL_GET(self->ht->vals, idx);
    /* template! PyObject* res = \([.val, "val"] | to_py); */
    PyObject* res = PyLong_FromLongLong(val);
    mdict_remove_item(self->ht, idx);
    return res;
}

/**
 * dict.popitem() invokes this function.
 */
static PyObject* popitem(dictObj* self) {
    h_t* h = self->ht;
    uint32_t idx;
    if (!mdict_prepare_remove_item(h, &idx)) {
        PyErr_SetString(PyExc_KeyError, "The map is empty");
        return NULL;
    }
    k_t key = KEY_GET(h->keys, idx);
    v_t val = VAL_GET(h->vals, idx);
    /* template! PyObject* key_obj = \([.key, "key"] | to_py); */
    PyObject* key_obj = PyUnicode_DecodeUTF8(key.ptr, key.len, NULL);
    /* template! PyObject* val_obj = \([.val, "val"] | to_py); */
    PyObject* val_obj = PyLong_FromLongLong(val);
    mdict_remove_item(h, idx);
    if (key_obj == NULL) {
        return NULL;
    }

    return PyTuple_Pack(2, key_obj, val_obj);
}

/**
 * This is invoked for the python expression d.setdefault(key, [default]). If no default is passed, zero is used.
 */
static PyObject* setdefault(dictObj* self, PyObject* args) {
    PyObject* key_obj;
    PyObject* val_obj = NULL;

    /* template! if (!PyArg_ParseTuple(args, \"O|O\", &key_obj, &val_obj)) { */
    if (!PyArg_ParseTuple(args, "O|O", &key_obj, &val_obj)) {
        return NULL;
    }

    k_t key;
    /* template(6)! \([.key, "key_obj", "key", "NULL", "key_len"] | from_py) */
    Py_ssize_t key_len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
    if (key.ptr == NULL) {
        return NULL;
    }
    key.len = key_len;

    /* template! v_t dfault = \(.val.zero); */
    v_t dfault = 0;
    if (val_obj != NULL) {
        /* template(4)! \([.val, "val_obj", "dfault", "NULL", "dfault_len"] | from_py) */
        dfault = PyLong_AsLongLong(val_obj);
        if (dfault == -1 && PyErr_Occurred()) {
            return NULL;
        }
    }

    pv_t previous;
    if (!mdict_set(self->ht, key, dfault, &previous, false)) {
        if (self->ht->error_code) {
            PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
            return NULL;
        }
        dfault = VAL_GET(&previous, 0);
    }
    /* template! return \([.val, "dfault"] | to_py); */
    return PyLong_FromLongLong(dfault);
}

/**
 * dict.clear() invokes this function.
 */
static PyObject* clear(dictObj* self) {
    mdict_clear(self->ht);
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
    /* template! \(if .key.disp == "str" then "Py_ssize_t key_len;" else "" end) */
    Py_ssize_t key_len;
    /* template(0)! \(if .val.disp == "str" then "Py_ssize_t val_len;" else "" end) */
    k_t key;
    v_t val;
    pv_t previous;
    while (PyDict_Next(dict, &pos, &key_obj, &value_obj)) {
        /* template(4)! \([.val, "value_obj", "val", "-1", "-"] | from_py) */
        val = PyLong_AsLongLong(value_obj);
        if (val == -1 && PyErr_Occurred()) {
            return -1;
        }

        /* template(5)! \([.key, "key_obj", "key", "-1", "-"] | from_py) */
        key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
        if (key.ptr == NULL) {
            return -1;
        }
        key.len = key_len;

        if (!mdict_set(self->ht, key, val, &previous, true)) {
            if (self->ht->error_code) {
                PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
                return -1;
            }

            VAL_UNSET(&previous, 0);
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
    pv_t previous;

    for (uint32_t i = 0; i < other->num_buckets; i++) {
        if (_bucket_is_live(other->flags, i)) {
            if (!mdict_set(h, KEY_GET(other->keys, i), VAL_GET(other->vals, i), &previous, true)) {
                if (self->ht->error_code) {
                    PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
                    return -1;
                }

                VAL_UNSET(&previous, 0);
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
    /* template(6)! \([.key, "key_obj", "key", "-1", "key_len"] | from_py) */
    Py_ssize_t key_len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
    if (key.ptr == NULL) {
        return -1;
    }
    key.len = key_len;

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
    /* template(6)! \([.key, "key_obj", "key", "NULL", "key_len"] | from_py) */
    Py_ssize_t key_len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
    if (key.ptr == NULL) {
        return NULL;
    }
    key.len = key_len;

    v_t val;
    if (!mdict_get(self->ht, key, &val)) {
        /* template! \([.key, "key"] | key_error); */
        PyErr_SetString(PyExc_KeyError, key.ptr);
        return NULL;
    }
    /* template! return \([.val, "val"] | to_py); */
    return PyLong_FromLongLong(val);
}

/**
 * This is invoked for the python expression d[key] = value. Both key and value must be of the hashtable type.
 * This is also invoke for del d[key], in which case the `val_obj` is NULL
 */
static int _setitem_(dictObj* self, PyObject* key_obj, PyObject* value_obj) {
    k_t key;
    /* template(6)! \([.key, "key_obj", "key", "-1", "key_len"] | from_py) */
    Py_ssize_t key_len;
    key.ptr = PyUnicode_AsUTF8AndSize(key_obj, &key_len);
    if (key.ptr == NULL) {
        return -1;
    }
    key.len = key_len;

    if (value_obj == NULL) {
        uint32_t idx;
        if (!mdict_prepare_remove(self->ht, key, &idx)) {
            /* template! \([.key, "key"] | key_error); */
            PyErr_SetString(PyExc_KeyError, key.ptr);
            return -1;
        }
        mdict_remove_item(self->ht, idx);
        return 0;
    }

    v_t val;
    /* template(4)! \([.val, "value_obj", "val", "-1"] | from_py) */
    val = PyLong_AsLongLong(value_obj);
    if (val == -1 && PyErr_Occurred()) {
        return -1;
    }

    pv_t previous;
    if (!mdict_set(self->ht, key, val, &previous, true)) {
        if (self->ht->error_code) {
            PyErr_SetString(PyExc_MemoryError, "Insufficient memory to reserve space");
            return -1;
        }

        VAL_UNSET(&previous, 0);
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

    PyObject* key_obj;
    /* template(0)! \(if .val.disp == "str" then "Py_ssize_t other_val_len;" else "" end) */
    v_t other_val;
    bool is_equal = true;
    h_t* h = self->ht;
    for (uint32_t i = 0; is_equal && i < h->num_buckets; i++) {
        if (_bucket_is_live(h->flags, i)) {
            k_t key = KEY_GET(h->keys, i);
            /* template! key_obj = \([.key, "key"] | to_py); */
            key_obj = PyUnicode_DecodeUTF8(key.ptr, key.len, NULL);
            PyObject* other_val_obj = PyObject_GetItem(other, key_obj);
            Py_CLEAR(key_obj);
            if (other_val_obj == NULL) {
                PyErr_Clear();
                is_equal = false;
                break;
            }
            /* template(2)! \([.val, "other_val_obj", "other_val"] | partial_from_py) */
            other_val = PyLong_AsLongLong(other_val_obj);
            if (other_val == -1 && PyErr_Occurred()) {
                PyErr_Clear();
                is_equal = false;
                break;
            }
            /* template(0)! \(if .val.disp == "str" then "other_val.len = other_val_len;" else "" end) */
            is_equal = VAL_EQ(VAL_GET(h->vals, i), other_val);
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
        /* template! return PyUnicode_FromString(\"<pypocketmap[\(.key.disp), \(.val.disp)]: {}>\"); */
        return PyUnicode_FromString("<pypocketmap[str, int64]: {}>");
    }
    /* template! const int REPR_DICT_POS = 1 + 12 + \(.key.disp | length) + 2 + \(.val.disp | length) + 3; */
    const int REPR_DICT_POS = 1 + 12 + 3 + 2 + 5 + 3;
    //               "<pypocketmap["   k ", "  v   "]: "
    /* template! const int REPR_MIN_PAIR = 2 + \(.key.short_repr_size) + \(.val.short_repr_size); */
    const int REPR_MIN_PAIR = 2 + 2 + 1;

    _PyUnicodeWriter writer;
    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;
    writer.min_length = REPR_DICT_POS + 1 + REPR_MIN_PAIR - 2 + REPR_MIN_PAIR * (h->size - 1) + 2;
    //            "<pypocketmap[_, _]" "{"  (k ": " v)          (", " k ": " v)*               "}>"

    /* template! if (_PyUnicodeWriter_WriteASCIIString(&writer, \"<pypocketmap[\(.key.disp), \(.val.disp)]: {\", REPR_DICT_POS + 1) < 0) { */
    if (_PyUnicodeWriter_WriteASCIIString(&writer, "<pypocketmap[str, int64]: {", REPR_DICT_POS + 1) < 0) {
        _PyUnicodeWriter_Dealloc(&writer);
        return NULL;
    }
    k_t key;
    v_t val;
    /* template(2)! \([.key, "key"] | repr_declare) */
    PyObject* key_obj = NULL;
    PyObject* key_repr;
    /* template! \([.val, "val"] | repr_declare) */
    char val_repr[48];
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
            key = KEY_GET(h->keys, i);
            /* template(17)! \([.key, "key"] | repr_write) */
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

            val = VAL_GET(h->vals, i);
            /* template(5)! \([.val, "val"] | repr_write) */
            size_t val_len = snprintf(val_repr, 47, "%lld", val);
            if (_PyUnicodeWriter_WriteASCIIString(&writer, val_repr, val_len) < 0) {
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
    /* template! return iter_new(self, &keyIterType_\(.key.disp)_\(.val.disp)); */
    return iter_new(self, &keyIterType_str_int64);
}

/**
 * Returns the value iterator
 */
static PyObject* values(dictObj* self) {
    /* template! return iter_new(self, &valueIterType_\(.key.disp)_\(.val.disp)); */
    return iter_new(self, &valueIterType_str_int64);
}

/**
 * Returns the item iterator
 */
static PyObject* items(dictObj* self) {
    /* template! return iter_new(self, &itemIterType_\(.key.disp)_\(.val.disp)); */
    return iter_new(self, &itemIterType_str_int64);
}

/**
 * Returns a new pypocketmap containing all items present in this hashtable when dict.copy() is called.
 */
static PyObject* copy(dictObj* self) {
    PyObject* args = Py_BuildValue("(I)", self->ht->num_buckets);
    dictObj* new_obj = (dictObj *) PyObject_CallObject((PyObject *)((PyObject *) self)->ob_type, args);
    Py_DECREF(args);
    if (new_obj == NULL) {
        return NULL;
    }
    _update_from_mdict(new_obj, self);
    return (PyObject*) new_obj;
}

static PyObject* update(dictObj* self, PyObject* args);

/* template! static PyMethodDef methods_\(.key.disp)_\(.val.disp)[] = { */
static PyMethodDef methods_str_int64[] = {
    {"get", (PyCFunction)get, METH_VARARGS, "Return the value for `key` if `key` is in the dictionary, else `default`. If `default` is not given, it defaults to None, so that this method never raises a KeyError."},
    {"pop", (PyCFunction)pop, METH_VARARGS, "If key is in the dictionary, remove it and return its value, else return `default`. If `default` is not given and `key` is not in the dictionary, a KeyError is raised."},
    {"popitem", (PyCFunction)popitem, METH_NOARGS, "Remove and return a (key, value) pair from the dictionary."},
    {"setdefault", (PyCFunction)setdefault, METH_VARARGS, "If `key` is in the dictionary, return its value. If not, insert `key` with a value of `default` and return `default`. default defaults to 0."},
    {"clear", (PyCFunction)clear, METH_NOARGS, "Remove all items from the dictionary."},
    {"update", (PyCFunction)update, METH_VARARGS, "Updates the map with all key-value pairs within the given input."},
    {"keys", (PyCFunction)keys, METH_NOARGS, "Returns an iterator over the map's keys"},
    {"values", (PyCFunction)values, METH_NOARGS, "Returns an iterator over the map's values"},
    {"items", (PyCFunction)items, METH_NOARGS, "Returns an iterator over the map's (key, value) pairs"},
    {"copy", (PyCFunction)copy, METH_NOARGS, "Returns a deep copy of the hashtable"},
    {NULL, NULL, 0, NULL}
};

/* template! static PySequenceMethods sequence_\(.key.disp)_\(.val.disp) = { */
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

/* template! static PyMappingMethods mapping_\(.key.disp)_\(.val.disp) = { */
static PyMappingMethods mapping_str_int64 = {
    (lenfunc) _len_, /*mp_length*/
    (binaryfunc)_getitem_, /*mp_subscript*/
    (objobjargproc)_setitem_, /*mp_ass_subscript*/
};

/* template! static PyTypeObject dictType_\(.key.disp)_\(.val.disp) = { */
static PyTypeObject dictType_str_int64 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    /* template(5)! .tp_name = \"pypocketmap[\(.key.disp), \(.val.disp)]\",\n.tp_doc = \"pypocketmap[\(.key.disp), \(.val.disp)]\",\n.tp_as_sequence = &sequence_\(.key.disp)_\(.val.disp),\n.tp_as_mapping = &mapping_\(.key.disp)_\(.val.disp),\n.tp_methods = methods_\(.key.disp)_\(.val.disp), */
    .tp_name = "pypocketmap[str, int64]",
    .tp_doc = "pypocketmap[str, int64]",
    .tp_as_sequence = &sequence_str_int64,
    .tp_as_mapping = &mapping_str_int64,
    .tp_methods = methods_str_int64,
    .tp_basicsize = sizeof(dictObj),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = (newfunc) custom_new,
    .tp_init = (initproc) custom_init,
    .tp_dealloc = (destructor) custom_dealloc,
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

        /* template! if (PyObject_IsInstance(other, (PyObject *) &dictType_\(.key.disp)_\(.val.disp)) != 1) { */
        if (PyObject_IsInstance(other, (PyObject *) &dictType_str_int64) != 1) {
            /* template! PyErr_SetString(PyExc_TypeError, \"Argument needs to be either a pypocketmap[\(.key.disp), \(.val.disp)] or compatible Python dictionary\"); */
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

/* template(4)! static struct PyModuleDef moduleDef_\(.key.disp)_\(.val.disp) = {\n    PyModuleDef_HEAD_INIT,\n    \"\(.key.disp)_\(.val.disp)\", // name of module\n    \"pypocketmap[\(.key.disp), \(.val.disp)]\", // Documentation of the module */
static struct PyModuleDef moduleDef_str_int64 = {
    PyModuleDef_HEAD_INIT,
    "str_int64", // name of module
    "pypocketmap[str, int64]", // Documentation of the module
    -1,   // size of per-interpreter state of the module, or -1 if the module keeps state in global variables
};

/* template! PyMODINIT_FUNC PyInit_\(.key.disp)_\(.val.disp)(void) { */
PyMODINIT_FUNC PyInit_str_int64(void) {
    PyObject* obj;

    /* template! if (PyType_Ready(&dictType_\(.key.disp)_\(.val.disp)) < 0) */
    if (PyType_Ready(&dictType_str_int64) < 0)
        return NULL;

    /* template! if (PyType_Ready(&keyIterType_\(.key.disp)_\(.val.disp)) < 0) */
    if (PyType_Ready(&keyIterType_str_int64) < 0)
        return NULL;

    /* template! if (PyType_Ready(&valueIterType_\(.key.disp)_\(.val.disp)) < 0) */
    if (PyType_Ready(&valueIterType_str_int64) < 0)
        return NULL;

    /* template! if (PyType_Ready(&itemIterType_\(.key.disp)_\(.val.disp)) < 0) */
    if (PyType_Ready(&itemIterType_str_int64) < 0)
        return NULL;

    /* template! obj = PyModule_Create(&moduleDef_\(.key.disp)_\(.val.disp)); */
    obj = PyModule_Create(&moduleDef_str_int64);
    if (obj == NULL)
        return NULL;

    /* template(3)! Py_INCREF(&dictType_\(.key.disp)_\(.val.disp));\nif (PyModule_AddObject(obj, \"create\", (PyObject *) &dictType_\(.key.disp)_\(.val.disp)) < 0) {\n    Py_DECREF(&dictType_\(.key.disp)_\(.val.disp)); */
    Py_INCREF(&dictType_str_int64);
    if (PyModule_AddObject(obj, "create", (PyObject *) &dictType_str_int64) < 0) {
        Py_DECREF(&dictType_str_int64);
        Py_DECREF(obj);
        return NULL;
    }

    return obj;
}
