#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "./flags.h"
#include "./bits.h"
#include "./simd.h"

#ifndef KEY_TYPE_TAG
#define KEY_TYPE_TAG TYPE_TAG_STR
#define VAL_TYPE_TAG TYPE_TAG_I32
#endif

#if KEY_TYPE_TAG == TYPE_TAG_I32
typedef int32_t k_t;
typedef int32_t pk_t;
#define KEY_EQ(a, b) ((a) == (b))
static inline uint32_t _hash_func(k_t key) { return (uint32_t) key; }
static inline k_t _get_key(pk_t* keys, uint32_t idx) { return keys[idx]; }
static inline bool _set_key(pk_t* keys, uint32_t idx, k_t k) {
    keys[idx] = k;
    return true;
}
static inline void _unset_key(pk_t* keys, uint32_t idx) {}

#elif KEY_TYPE_TAG == TYPE_TAG_I64
typedef int64_t k_t;
typedef int64_t pk_t;
#define KEY_EQ(a, b) ((a) == (b))
static inline uint32_t _hash_func(k_t key) { return ((uint32_t) key) ^ ((uint32_t) (key >> 32)); }
static inline k_t _get_key(pk_t* keys, uint32_t idx) { return keys[idx]; }
static inline bool _set_key(pk_t* keys, uint32_t idx, k_t k) {
    keys[idx] = k;
    return true;
}
static inline void _unset_key(pk_t* keys, uint32_t idx) {}

#elif KEY_TYPE_TAG == TYPE_TAG_F32
typedef float k_t;
typedef float pk_t;
#define KEY_EQ(a, b) ((a) == (b))
static inline uint32_t _hash_func(k_t key) {
    uint32_t ikey = *((uint32_t*) &key);
    return ikey ^ (ikey >> 16);
}
static inline k_t _get_key(pk_t* keys, uint32_t idx) { return keys[idx]; }
static inline bool _set_key(pk_t* keys, uint32_t idx, k_t k) {
    keys[idx] = k;
    return true;
}
static inline void _unset_key(pk_t* keys, uint32_t idx) {}

#elif KEY_TYPE_TAG == TYPE_TAG_F64
typedef double k_t;
typedef double pk_t;
#define KEY_EQ(a, b) ((a) == (b))
static inline uint32_t _hash_func(k_t key) {
    uint64_t ikey64 = *((uint64_t*) &key);
    uint32_t ikey = ((uint32_t) ikey) ^ ((uint32_t) (ikey >> 32))
    return ikey ^ (ikey >> 16);
}
static inline k_t _get_key(pk_t* keys, uint32_t idx) { return keys[idx]; }
static inline bool _set_key(pk_t* keys, uint32_t idx, k_t k) {
    keys[idx] = k;
    return true;
}
static inline void _unset_key(pk_t* keys, uint32_t idx) {}

#elif KEY_TYPE_TAG == TYPE_TAG_STR
typedef struct {
    const char* ptr;
    uint64_t len;
} k_t;
typedef struct {
    char data[15];
    uint8_t meta;
} pk_contained;
typedef struct {
    char* ptr;
#if UINTPTR_MAX == 0xffffffff
        uint32_t __pad;
#endif
    uint64_t meta;
} pk_spilled;
typedef union {
    pk_contained contained;
    pk_spilled spilled;
} pk_t;
#define KEY_EQ(a, b) ((a.len == b.len) && memcmp(a.ptr, b.ptr, a.len) == 0)
#define KEYS_POINT 1
#include "./wyhash.h"
const uint64_t transparent_xor[5] = {0, 0, 0, 0, 0};
static inline uint32_t _hash_func(k_t key) {
    return (uint32_t) (wyhash((void*) key.ptr, key.len, 1, transparent_xor));
}
static inline k_t _get_key(pk_t* keys, uint32_t idx) {
    k_t res;
    if (keys[idx].contained.meta & 1) {
        res.ptr = keys[idx].contained.data;
        res.len = keys[idx].contained.meta >> 1;
        return res;
    }
    res.ptr = keys[idx].spilled.ptr;
    res.len = keys[idx].spilled.meta >> 1;
    return res;
}
static inline bool _set_key(pk_t* keys, uint32_t idx, k_t k) {
    if (k.len < 15) {
        memcpy(keys[idx].contained.data, k.ptr, k.len+1);
        keys[idx].contained.meta = (k.len << 1) | 1;
    } else {
        keys[idx].spilled.ptr = (char*) malloc(k.len+1);
        if (keys[idx].spilled.ptr == NULL) return false;
        memcpy(keys[idx].spilled.ptr, k.ptr, k.len+1);
        keys[idx].spilled.meta = k.len << 1;
    }
    return true;
}
static inline void _unset_key(pk_t* keys, uint32_t idx) {
    if (!(keys[idx].contained.meta & 1)) {
        free(keys[idx].spilled.ptr);
    }
}
#endif

#if VAL_TYPE_TAG == TYPE_TAG_I32
typedef int32_t v_t;
#elif VAL_TYPE_TAG == TYPE_TAG_I64
typedef int64_t v_t;
#elif VAL_TYPE_TAG == TYPE_TAG_F32
typedef float v_t;
#elif VAL_TYPE_TAG == TYPE_TAG_F64
typedef double v_t;
#elif VAL_TYPE_TAG == TYPE_TAG_STR
typedef char* v_t;
#endif

const double PEAK_LOAD = 0.79;

typedef struct {
    uint32_t num_buckets;
    uint32_t num_deleted;
    uint32_t size;
    uint32_t upper_bound;  // floor(PEAK_LOAD * num_buckets)
    uint32_t grow_threshold;  // size below this threshold when hitting upper_bound means rehash at eq num_buckets
    uint32_t seed;
    uint64_t *flags;  // each 8 bits refers to a bucket; see simd constants
    pk_t *keys;
    v_t *vals;
    int error_code;
    bool is_map;
} h_t;

static inline bool _bucket_is_live(const uint64_t *flags, uint32_t i) {
    return !((flags[i>>3] >> (8*(i&7))) & 128);
}

static inline void _bucket_set(uint64_t *flags, uint32_t i, uint8_t v) {
    uint64_t v_shifted = ((uint64_t) v) << (8*(i&7));
    uint64_t set_mask = 0xffULL << (8*(i&7));
    flags[i>>3] ^= (flags[i>>3] ^ v_shifted) & set_mask;
    return;
}

static inline uint32_t _flags_size(uint32_t num_buckets) {
    return num_buckets >> 3;
}

static inline uint32_t _match_index(uint32_t flags_index, uint32_t offset) {
    return (flags_index << 3) + offset;
}

static int _mdict_resize(h_t* h, uint32_t new_num_buckets);

static h_t* mdict_create(uint32_t num_buckets, bool is_map) {
    h_t* h = (h_t*)calloc(1, sizeof(h_t));

    h->size = 0;
    h->num_deleted = 0;
    h->is_map = is_map;
    if (num_buckets < 32) {
        if (_mdict_resize(h, 32) == -1) {
            free(h);
            return NULL;
        }
    } else {
        uint32_t initial = 1u << (32 - count_leading_zeroes_unchecked32(num_buckets - 1));
        if (_mdict_resize(h, initial) == -1) {
            free(h);
            return NULL;
        }
    }

    return h;
}

static void _mdict_clear_keys(h_t* h) {
    for (uint32_t j = 0; j < h->num_buckets; ++j) {
        if (_bucket_is_live(h->flags, j)) {
            _unset_key(h->keys, j);
        }
    }
}

static void mdict_destroy(h_t* h) {
    if (h) {
#ifdef KEYS_POINT
        _mdict_clear_keys(h);
#endif
        free((void *)h->keys);
        free(h->flags);
        free((void *)h->vals);
        free(h);
    }
}

static inline int32_t _mdict_read_index(h_t* h, k_t key, uint32_t hash_upper, uint32_t h2) {
    const uint32_t step_basis = GROUP_WIDTH >> 3;
    uint32_t mask = _flags_size(h->num_buckets) - 1;
    mask &= ~(step_basis - 1);  // e.g. mask should select 0,2,4,6 if 64 buckets, flags_size 8, num_groups 4
    uint32_t flags_index = hash_upper & mask;
    uint32_t step = step_basis;

    // this should loop `num_groups` times
    //  1. mask + step_basis = flags_size
    //  2. flags_size / step_basis = num_groups
    while (step <= mask + step_basis) {
        g_t group = _group_load(&h->flags[flags_index]);
        gbits matches = _group_match(group, h2);
        while (_gbits_has_next(matches)) {
            uint32_t offset = _gbits_next(&matches);
            uint32_t index = _match_index(flags_index, offset);
            if (KEY_EQ(_get_key(h->keys, index), key)) {  // likely
                return index;
            }
        }
        gbits empties = _group_mask_empty(group);
        if (empties) {  // likely
            uint32_t offset = _gbits_next(&empties);
            return -((int32_t) _match_index(flags_index, offset)) - 1;
        }

        flags_index = (flags_index + step) & mask;
        step += step_basis;
    }
    assert(false);
    return -h->num_buckets - 1;
}

// Caller is responsible for rehashing and freeing previous keys,values,flags
static int _mdict_resize(h_t* h, uint32_t new_num_buckets) {
    uint64_t* new_flags = (uint64_t*) calloc(_flags_size(new_num_buckets), sizeof(uint64_t));

    if (!new_flags)
        return -1;

    memset(new_flags, FLAGS_EMPTY, _flags_size(new_num_buckets) * sizeof(uint64_t));

    // TODO can probably realloc when growing
    pk_t *new_keys = (pk_t*)calloc(new_num_buckets, sizeof(pk_t));
    if (!new_keys) {
        free(new_flags);
        return -1;
    }
    h->keys = new_keys;
    if (h->is_map) {
        v_t *new_vals = (v_t*)calloc(new_num_buckets, sizeof(v_t));
        if (!new_vals) {
            free(new_flags);
            free(new_keys);
            return -1;
        }
        h->vals = new_vals;
    }

    h->flags = new_flags;
    h->num_buckets = new_num_buckets;
    h->num_deleted = 0;
    h->upper_bound = (uint32_t)(h->num_buckets * PEAK_LOAD);
    h->grow_threshold = (uint32_t)(h->num_buckets * PEAK_LOAD * PEAK_LOAD);

    return 0;
}

static void _mdict_resize_rehash(h_t* h, uint32_t new_num_buckets) {
    uint32_t old_num_buckets = h->num_buckets;
    pk_t* old_keys = h->keys;
    v_t* old_vals = h->vals;
    uint64_t* old_flags = h->flags;

    const uint32_t step_basis = GROUP_WIDTH >> 3;
    uint32_t new_mask = _flags_size(new_num_buckets) - 1;
    new_mask &= ~(step_basis - 1);  // e.g. mask should select 0,2,4,6 if 64 buckets, flags_size 8, num_groups 4
    if (_mdict_resize(h, new_num_buckets) == -1) {
        free(old_flags);
        free(old_keys);
        free(old_vals);
        h->error_code = -1;
    }

    for (uint32_t j = 0; j < old_num_buckets; ++j) {
        if (_bucket_is_live(old_flags, j)) {
            pk_t key = old_keys[j];
            v_t val;
            if (h->is_map) {
                val = old_vals[j];
            }
            uint32_t hash = _hash_func(_get_key(old_keys, j));
            uint32_t flags_index = (hash >> 7) & new_mask;
            uint32_t h2 = hash & 0x7f;
            uint32_t step = step_basis;

            while (step <= new_mask + step_basis) {
                g_t group = _group_load(&h->flags[flags_index]);
                gbits empties = _group_mask_empty(group);
                if (empties) {  // likely
                    uint32_t offset = _gbits_next(&empties);
                    uint32_t new_index = _match_index(flags_index, offset);
                    _bucket_set(h->flags, new_index, h2);
                    h->keys[new_index] = key;
                    if (h->is_map) {
                        h->vals[new_index] = val;
                    }
                    break;
                }

                flags_index = (flags_index + step) & new_mask;
                step += step_basis;
            }
            assert(step <= new_mask + step_basis);
        }
    }

    free(old_flags);
    free(old_keys);
    free(old_vals);
}

static void mdict_clear(h_t* h) {
    memset(h->flags, FLAGS_EMPTY, _flags_size(h->num_buckets) * sizeof(uint64_t));
    h->size = 0;
    h->num_deleted = 0;
}

static inline bool mdict_set(h_t* h, k_t key, v_t val, v_t* val_box, bool should_replace) {
    if (h->size + h->num_deleted >= h->upper_bound) {
        uint32_t new_num_buckets = (h->size >= h->grow_threshold) ? (h->num_buckets << 1) : h->num_buckets;
        _mdict_resize_rehash(h, new_num_buckets);
        if (h->error_code) {
            return false;
        }
    }

    uint32_t hash = _hash_func(key);
    // copy of _mdict_read_index, but modified to SIMD store after break
    uint32_t h2 = hash & 0x7f;
    const uint32_t step_basis = GROUP_WIDTH >> 3;
    uint32_t mask = _flags_size(h->num_buckets) - 1;
    mask &= ~(step_basis - 1);  // e.g. mask should select 0,2,4,6 if 64 buckets, flags_size 8, num_groups 4
    uint32_t flags_index = (hash >> 7) & mask;
    uint32_t step = step_basis;

    // this should loop `num_groups` times
    //  1. mask + step_basis = flags_size
    //  2. flags_size / step_basis = num_groups
    g_t group;
    uint32_t offset;
    while (step <= mask + step_basis) {
        group = _group_load(&h->flags[flags_index]);
        gbits matches = _group_match(group, h2);
        while (_gbits_has_next(matches)) {
            offset = _gbits_next(&matches);
            uint32_t index = _match_index(flags_index, offset);
            if (KEY_EQ(_get_key(h->keys, index), key)) {  // likely
                if (val_box != NULL) {
                    *val_box = h->vals[index];
                }
                if (should_replace) {
                    h->vals[index] = val;
                }
                return false;
            }
        }
        gbits empties = _group_mask_empty(group);
        if (empties) {  // likely
            offset = _gbits_next(&empties);
            break;
        }

        flags_index = (flags_index + step) & mask;
        step += step_basis;
    }

    _group_set(group, (int8_t*) &h->flags[flags_index], offset, h2);
    uint32_t idx = _match_index(flags_index, offset);
    if (!_set_key(h->keys, idx, key)) {
        h->error_code = -2;
        return false;
    }
    h->vals[idx] = val;
    h->size++;
    return true;
}

static inline bool mdict_remove(h_t* h, k_t key, v_t* val_box) {
    uint32_t hash = _hash_func(key);
    int32_t idx = _mdict_read_index(h, key, hash >> 7, hash & 0x7f);
    if (idx < 0) {
        return false;
    }

    _bucket_set(h->flags, idx, FLAGS_DELETED);
    _unset_key(h->keys, idx);
    if (val_box != NULL) {
        *val_box = h->vals[idx];
    }
    h->size--;
    h->num_deleted++;
    return true;
}

static inline bool mdict_get(h_t* h, k_t key, v_t* val_box) {
    uint32_t hash = _hash_func(key);
    int32_t idx = _mdict_read_index(h, key, hash >> 7, hash & 0x7f);
    if (idx < 0) {
        return false;
    }

    *val_box = h->vals[idx];
    return true;
}

static inline bool mdict_contains(h_t* h, k_t key) {
    uint32_t hash = _hash_func(key);
    return _mdict_read_index(h, key, hash >> 7, hash & 0x7f) >= 0;
}
