#ifndef PYPOCKET_PACKED_H_
#define PYPOCKET_PACKED_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static inline int32_t packed_get_i32(int32_t* arr, uint32_t idx) { return arr[idx]; }
static inline bool packed_set_i32(int32_t* arr, uint32_t idx, int32_t elem) {
    arr[idx] = elem;
    return true;
}
static inline void packed_unset_i32(int32_t* arr, uint32_t idx) {}

static inline int64_t packed_get_i64(int64_t* arr, uint32_t idx) { return arr[idx]; }
static inline bool packed_set_i64(int64_t* arr, uint32_t idx, int64_t elem) {
    arr[idx] = elem;
    return true;
}
static inline void packed_unset_i64(int64_t* arr, uint32_t idx) {}

static inline float packed_get_f32(float* arr, uint32_t idx) { return arr[idx]; }
static inline bool packed_set_f32(float* arr, uint32_t idx, float elem) {
    arr[idx] = elem;
    return true;
}
static inline void packed_unset_f32(float* arr, uint32_t idx) {}

static inline double packed_get_f64(double* arr, uint64_t idx) { return arr[idx]; }
static inline bool packed_set_f64(double* arr, uint64_t idx, double elem) {
    arr[idx] = elem;
    return true;
}
static inline void packed_unset_f64(double* arr, uint64_t idx) {}

typedef struct {
    const char* ptr;
    uint64_t len;
} str_t;

// [0..14]: <data> 
// [15]   : 0b000LLLL1 (L bits are the length)
typedef struct {
    char data[15];
    uint8_t meta;
} packed_str_contained;

// [0..7]: <ptr> 
// [7..15]: 0bLLLL...LLL0 (L bits are the length)
//   - note that on a little endian system, the uint8_t overlapping packed_str_contained.meta
//     will be bits 64-57, not 7-0. that's okay because the critical bit at 57 is only `1` if
//     len >= 2**56, i.e. never
typedef struct {
    char* ptr;
#if UINTPTR_MAX == 0xffffffff
    uint32_t __pad;
#endif
    uint64_t meta;
} packed_str_spilled;

typedef union {
    packed_str_contained contained;
    packed_str_spilled spilled;
} packed_str_t;

static inline str_t packed_get_str(packed_str_t* arr, uint32_t idx) {
    str_t res;
    if (arr[idx].contained.meta & 1) {
        res.ptr = arr[idx].contained.data;
        res.len = arr[idx].contained.meta >> 1;
        return res;
    }
    res.ptr = arr[idx].spilled.ptr;
    res.len = arr[idx].spilled.meta >> 1;
    return res;
}
static inline bool packed_set_str(packed_str_t* arr, uint32_t idx, str_t elem) {
    if (elem.len < 15) {
        memcpy(arr[idx].contained.data, elem.ptr, elem.len+1);
        arr[idx].contained.meta = ((uint8_t) elem.len << 1) | 1;
    } else {
        arr[idx].spilled.ptr = (char*) malloc(elem.len+1);
        if (arr[idx].spilled.ptr == NULL) return false;
        memcpy(arr[idx].spilled.ptr, elem.ptr, elem.len+1);
        arr[idx].spilled.meta = elem.len << 1;
    }
    return true;
}
static inline void packed_unset_str(packed_str_t* arr, uint32_t idx) {
    if (!(arr[idx].contained.meta & 1)) {
        free(arr[idx].spilled.ptr);
    }
}

#endif // PYPOCKET_PACKED_H_
