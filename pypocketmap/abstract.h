#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "./flags.h"
#include "./bits.h"
#include "./simd.h"
#include "./packed.h"
#include "./optimization.h"

#ifndef KEY_TYPE_TAG
#define KEY_TYPE_TAG TYPE_TAG_STR
#define VAL_TYPE_TAG TYPE_TAG_STR
#endif

#if KEY_TYPE_TAG == TYPE_TAG_I32
typedef int32_t k_t;
typedef int32_t pk_t;
typedef bool hasher_t;
#define KEY_EQ(a, b) ((a) == (b))
#define KEY_GET(arr, idx) packed_get_i32(arr, idx)
#define KEY_SET(arr, idx, elem) packed_set_i32(arr, idx, elem)
#define KEY_UNSET(arr, idx) packed_unset_i32(arr, idx)
static inline uint32_t _hash_func(hasher_t* _, k_t key) { return (uint32_t) key; }
static inline void _hasher_init() {}

#elif KEY_TYPE_TAG == TYPE_TAG_I64
typedef int64_t k_t;
typedef int64_t pk_t;
typedef bool hasher_t;
#define KEY_EQ(a, b) ((a) == (b))
#define KEY_GET(arr, idx) packed_get_i64(arr, idx)
#define KEY_SET(arr, idx, elem) packed_set_i64(arr, idx, elem)
#define KEY_UNSET(arr, idx) packed_unset_i64(arr, idx)
static inline uint32_t _hash_func(hasher_t* _, k_t key) {
    // originally this was just (high bits xor low bits); however we need
    // `entry.h2 == query_h2` to correlate very strongly with `entry == query`,
    // which is broken if the keys are fairly dense: all of
    // [a*128, (a+GROUP_SIZE)*128) has the same initial search group `a % num_groups`.
    //
    // resize_rehash also took way longer before this but I don't have an explanation

    // https://github.com/cbreeden/fxhash/blob/master/lib.rs
    uint32_t state = (uint32_t) key;
    state *= 0x9e3779b9UL;
    state = (state << 5) ^ ((uint32_t) (key >> 32));
    return state * 0x9e3779b9UL;
}
static inline void _hasher_init() {}

#elif KEY_TYPE_TAG == TYPE_TAG_F32
typedef float k_t;
typedef float pk_t;
typedef bool hasher_t;
#define KEY_EQ(a, b) ((a) == (b))
#define KEY_GET(arr, idx) packed_get_f32(arr, idx)
#define KEY_SET(arr, idx, elem) packed_set_f32(arr, idx, elem)
#define KEY_UNSET(arr, idx) packed_unset_f32(arr, idx)
static inline uint32_t _hash_func(hasher_t* _, k_t key) {
    uint32_t ikey = *((uint32_t*) &key);
    return ikey ^ (ikey >> 16);
}
static inline void _hasher_init() {}

#elif KEY_TYPE_TAG == TYPE_TAG_F64
typedef double k_t;
typedef double pk_t;
typedef bool hasher_t;
#define KEY_EQ(a, b) ((a) == (b))
#define KEY_GET(arr, idx) packed_get_f64(arr, idx)
#define KEY_SET(arr, idx, elem) packed_set_f64(arr, idx, elem)
#define KEY_UNSET(arr, idx) packed_unset_f64(arr, idx)
static inline uint32_t _hash_func(hasher_t* _, k_t key) {
    uint64_t ikey64 = *((uint64_t*) &key);
    uint32_t ikey = ((uint32_t) ikey) ^ ((uint32_t) (ikey >> 32))
    return ikey ^ (ikey >> 16);
}
static inline void _hasher_init() {}

#elif KEY_TYPE_TAG == TYPE_TAG_STR
#include "./polymur-hash.h"

typedef str_t k_t;
typedef packed_str_t pk_t;
typedef PolymurHashParams hasher_t;
#define KEY_EQ(a, b) ((a.len == b.len) && memcmp(a.ptr, b.ptr, a.len) == 0)
#define KEY_GET(arr, idx) packed_get_str(arr, idx)
#define KEY_SET(arr, idx, elem) packed_set_str(arr, idx, elem)
#define KEY_UNSET(arr, idx) packed_unset_str(arr, idx)
#define KEYS_POINT 1

static inline uint32_t _hash_func(hasher_t* hasher, k_t key) {
    return (uint32_t) (polymur_hash((uint8_t*) key.ptr, key.len, hasher, 0));
}
static inline void _hasher_init(hasher_t* hasher) {
    polymur_init_params_from_seed(hasher, 0xfedbca9876543210ULL);
}
#endif

#if VAL_TYPE_TAG == TYPE_TAG_I32
typedef int32_t v_t;
typedef int32_t pv_t;
#define VAL_EQ(a, b) ((a) == (b))
#define VAL_GET(arr, idx) packed_get_i32(arr, idx)
#define VAL_SET(arr, idx, elem) packed_set_i32(arr, idx, elem)
#define VAL_UNSET(arr, idx) packed_unset_i32(arr, idx)

#elif VAL_TYPE_TAG == TYPE_TAG_I64
typedef int64_t v_t;
typedef int64_t pv_t;
#define VAL_EQ(a, b) ((a) == (b))
#define VAL_GET(arr, idx) packed_get_i64(arr, idx)
#define VAL_SET(arr, idx, elem) packed_set_i64(arr, idx, elem)
#define VAL_UNSET(arr, idx) packed_unset_i64(arr, idx)

#elif VAL_TYPE_TAG == TYPE_TAG_F32
typedef float v_t;
typedef float pv_t;
#define VAL_EQ(a, b) ((a) == (b))
#define VAL_GET(arr, idx) packed_get_f32(arr, idx)
#define VAL_SET(arr, idx, elem) packed_set_f32(arr, idx, elem)
#define VAL_UNSET(arr, idx) packed_unset_f32(arr, idx)

#elif VAL_TYPE_TAG == TYPE_TAG_F64
typedef double v_t;
typedef double pv_t;
#define VAL_EQ(a, b) ((a) == (b))
#define VAL_GET(arr, idx) packed_get_f64(arr, idx)
#define VAL_SET(arr, idx, elem) packed_set_f64(arr, idx, elem)
#define VAL_UNSET(arr, idx) packed_unset_f64(arr, idx)

#elif VAL_TYPE_TAG == TYPE_TAG_STR
typedef str_t v_t;
typedef packed_str_t pv_t;
#define VAL_EQ(a, b) ((a.len == b.len) && memcmp(a.ptr, b.ptr, a.len) == 0)
#define VAL_GET(arr, idx) packed_get_str(arr, idx)
#define VAL_SET(arr, idx, elem) packed_set_str(arr, idx, elem)
#define VAL_UNSET(arr, idx) packed_unset_str(arr, idx)
#define VALS_POINT 1

#endif

const double PEAK_LOAD = 0.79;
const char* const EMPTY_STR = "";

typedef struct {
    uint64_t *flags;  // each 8 bits refers to a bucket; see simd constants
    pk_t *keys;
    pv_t *vals;
    uint32_t num_buckets;
    uint32_t num_deleted;
    uint32_t size;
    uint32_t upper_bound;  // floor(PEAK_LOAD * num_buckets)
    uint32_t grow_threshold;  // size below this threshold when hitting upper_bound means rehash at eq num_buckets
    int error_code;
    hasher_t hasher;
    bool is_map;
} h_t;

static inline bool _bucket_is_live(const uint64_t *flags, uint32_t i) {
    return !((flags[i>>3] >> (8*(i&7))) & 128);
}

static inline bool _bucket_is_deleted(const uint64_t *flags, uint32_t i) {
    return ((flags[i>>3] >> (8*(i&7))) & 0xff) == FLAGS_DELETED;
}

static inline void _bucket_set(uint64_t *flags, uint32_t i, uint8_t v) {
    uint64_t v_shifted = ((uint64_t) v) << (8*(i&7));
    uint64_t set_mask = 0xffULL << (8*(i&7));
    flags[i>>3] ^= (flags[i>>3] ^ v_shifted) & set_mask;
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
    h->error_code = 0;
    h->is_map = is_map;
    _hasher_init(&h->hasher);
    h->flags = NULL;
    h->keys = NULL;
    h->vals = NULL;
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
    memset(h->flags, FLAGS_EMPTY, _flags_size(h->num_buckets) * sizeof(uint64_t));

    return h;
}

static void mdict_destroy(h_t* h) {
    if (h) {
#if defined(KEYS_POINT) || defined(VALS_POINT)
        for (uint32_t j = 0; j < h->num_buckets; ++j) {
            if (_bucket_is_live(h->flags, j)) {
                KEY_UNSET(h->keys, j);
                VAL_UNSET(h->vals, j);
            }
        }
#endif
        free(h->flags);
        free((void *)h->keys);
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
            if (ABSL_PREDICT_TRUE(KEY_EQ(KEY_GET(h->keys, index), key))) {
                return index;
            }
        }
        gbits empties = _group_mask_empty(group);
        if (ABSL_PREDICT_TRUE(empties)) {
            uint32_t offset = _gbits_next(&empties);
            return -((int32_t) _match_index(flags_index, offset)) - 1;
        }

        flags_index = (flags_index + step) & mask;
        step += step_basis;
    }
    assert(false);
    return -((int32_t) h->num_buckets) - 1;
}

// Caller is responsible for rehashing and clearing anything currently marked deleted
static int _mdict_resize(h_t* h, uint32_t new_num_buckets) {
    uint64_t* new_flags = (uint64_t*) realloc((void*) h->flags, _flags_size(new_num_buckets) * sizeof(uint64_t));

    if (!new_flags) {
        return -1;
    }

    pk_t* new_keys = (pk_t*) realloc((void*) h->keys, new_num_buckets * sizeof(pk_t));

    if (!new_keys) {
        free(new_flags);
        return -1;
    }
    h->keys = new_keys;

    if (h->is_map) {
        pv_t* new_vals = (pv_t*) realloc((void*) h->vals, new_num_buckets * sizeof(pv_t));

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
    uint32_t old_flags_size = _flags_size(old_num_buckets);

    const uint32_t step_basis = GROUP_WIDTH >> 3;
    uint32_t new_flags_size = _flags_size(new_num_buckets);
    uint32_t new_mask = new_flags_size - 1;
    new_mask &= ~(step_basis - 1);  // e.g. mask should select 0,2,4,6 if 64 buckets, flags_size 8, num_groups 4
    if (_mdict_resize(h, new_num_buckets) == -1) {
        h->error_code = -1;
    }

    for (uint32_t flags_index = 0; flags_index < old_flags_size; flags_index += step_basis) {
        g_t group = _group_load(&h->flags[flags_index]);
        _group_convert_special_to_empty_and_full_to_deleted(group, (int8_t*) &h->flags[flags_index]);
    }
    memset((void*) &h->flags[old_flags_size], FLAGS_EMPTY, (new_flags_size - old_flags_size) * sizeof(uint64_t));

    uint32_t j = 0;
    while (j < old_num_buckets) {
        if (_bucket_is_deleted(h->flags, j)) {
            pk_t key = h->keys[j];
            pv_t val;
            if (h->is_map) {
                val = h->vals[j];
            }
            uint32_t hash = _hash_func(&h->hasher, KEY_GET(h->keys, j));
            uint32_t flags_index = (hash >> 7) & new_mask;
            uint32_t h2 = hash & 0x7f;
            uint32_t step = step_basis;
            if ((j >> 3) == flags_index) {
                _bucket_set(h->flags, j, h2);
                j += 1;
                continue;
            }

            while (step <= new_mask + step_basis) {
                g_t group = _group_load(&h->flags[flags_index]);
                gbits empties = _group_mask_empty_or_deleted(group);
                if (empties) {  // likely
                    uint32_t offset = _gbits_next(&empties);
                    uint32_t new_index = _match_index(flags_index, offset);
                    if (_bucket_is_deleted(h->flags, new_index)) {
                        // swap before writing then repeat on `j`
                        h->keys[j] = h->keys[new_index];
                        if (h->is_map) {
                            h->vals[j] = h->vals[new_index];
                        }
                        _bucket_set(h->flags, new_index, h2);
                        h->keys[new_index] = key;
                        if (h->is_map) {
                            h->vals[new_index] = val;
                        }
                    } else {
                        _bucket_set(h->flags, j, FLAGS_EMPTY);
                        _bucket_set(h->flags, new_index, h2);
                        h->keys[new_index] = key;
                        if (h->is_map) {
                            h->vals[new_index] = val;
                        }
                        j += 1;
                    }
                    break;
                }

                flags_index = (flags_index + step) & new_mask;
                step += step_basis;
            }
            assert(step <= new_mask + step_basis);
        } else {
            j += 1;
        }
    }
}

static void mdict_clear(h_t* h) {
#if defined(KEYS_POINT) || defined(VALS_POINT)
    for (uint32_t j = 0; j < h->num_buckets; ++j) {
        if (_bucket_is_live(h->flags, j)) {
            KEY_UNSET(h->keys, j);
            VAL_UNSET(h->vals, j);
        }
    }
#endif
    memset(h->flags, FLAGS_EMPTY, _flags_size(h->num_buckets) * sizeof(uint64_t));
    h->size = 0;
    h->num_deleted = 0;
}

// Returns true if the set is an _insert_, false if it is a _replace_ or an error occurred.
// Caller is responsible for freeing the value placed in val_box if VALS_POINT is defined.
static inline bool mdict_set(h_t* h, k_t key, v_t val, pv_t* val_box, bool should_replace) {
    if (h->size + h->num_deleted >= h->upper_bound) {
        uint32_t new_num_buckets = (h->size >= h->grow_threshold) ? (h->num_buckets << 1) : h->num_buckets;
        _mdict_resize_rehash(h, new_num_buckets);
        if (h->error_code) {
            return false;
        }
    }

    uint32_t hash = _hash_func(&h->hasher, key);
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
    while (true) {
        group = _group_load(&h->flags[flags_index]);
        gbits matches = _group_match(group, h2);
        while (_gbits_has_next(matches)) {
            offset = _gbits_next(&matches);
            uint32_t index = _match_index(flags_index, offset);
            if (ABSL_PREDICT_TRUE(KEY_EQ(KEY_GET(h->keys, index), key))) {
                if (val_box != NULL) {
                    *val_box = h->vals[index];
                }
                if (should_replace) {
                    VAL_SET(h->vals, index, val);
                }
                return false;
            }
        }
        gbits empties = _group_mask_empty(group);
        if (ABSL_PREDICT_TRUE(empties)) {
            offset = _gbits_next(&empties);
            break;
        }

        flags_index = (flags_index + step) & mask;
        step += step_basis;
    }

    _group_set(group, (int8_t*) &h->flags[flags_index], offset, h2);
    uint32_t idx = _match_index(flags_index, offset);
    if (!KEY_SET(h->keys, idx, key)) {
        h->error_code = -2;
        return false;
    }
    if (!VAL_SET(h->vals, idx, val)) {
        h->error_code = -2;
        return false;
    }
    h->size++;
    return true;
}

static inline bool mdict_prepare_remove(h_t* h, k_t key, uint32_t* idx_box) {
    uint32_t hash = _hash_func(&h->hasher, key);
    int32_t idx = _mdict_read_index(h, key, hash >> 7, hash & 0x7f);
    if (idx < 0) {
        return false;
    }
    *idx_box = (uint32_t) idx;
    return true;
}

static inline bool mdict_prepare_remove_item(h_t* h, uint32_t* idx_box) {
    if (h->size == 0) {
        return false;
    }
    uint32_t mask = h->num_buckets - 1;

    for (uint32_t idx = rand() & mask, ct = 0; ct <= mask; idx = (idx + 1) & mask, ct++) {
        if (_bucket_is_live(h->flags, idx)) {
            *idx_box = idx;
            return true;
        }
    }

    assert(false);
    return false;
}

static inline void mdict_remove_item(h_t* h, uint32_t idx) {
    KEY_UNSET(h->keys, idx);
    VAL_UNSET(h->vals, idx);
    _bucket_set(h->flags, idx, FLAGS_DELETED);
    h->size--;
    h->num_deleted++;
}

static inline bool mdict_get(h_t* h, k_t key, v_t* val_box) {
    uint32_t hash = _hash_func(&h->hasher, key);
    int32_t idx = _mdict_read_index(h, key, hash >> 7, hash & 0x7f);
    if (idx < 0) {
        return false;
    }

    *val_box = VAL_GET(h->vals, idx);
    return true;
}

static inline bool mdict_contains(h_t* h, k_t key) {
    uint32_t hash = _hash_func(&h->hasher, key);
    return _mdict_read_index(h, key, hash >> 7, hash & 0x7f) >= 0;
}
