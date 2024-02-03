#include "clar.h"
#include <stdint.h>
#include <stdio.h>

#include "../flags.h"
#define KEY_TYPE_TAG TYPE_TAG_STR
#define VAL_TYPE_TAG TYPE_TAG_I64
#include "../abstract.h"

static h_t* m;
static k_t test_key;

k_t make_key(int i, k_t* mut_key) {
  mut_key->len = sprintf((char*)mut_key->ptr, "%d", i);
  return *mut_key;
}

#define KEY(i) (make_key(i, &test_key))

void test_str_int64__initialize(void) {
  m = mdict_create(32, true);
  test_key.ptr = malloc(4);
}

void test_str_int64__cleanup(void) {
  free((void*) test_key.ptr);
  mdict_destroy(m);
}

void test_str_int64__smallstr(void) {
  cl_assert_equal_i(sizeof(pk_contained), sizeof(pk_spilled));
}

void test_str_int64__lots_of_insertions(void) {
  v_t v;
  for (int loop = 0; loop < 10; loop++) {
    cl_assert(m->size == 0);

    int count = 201;
    for (int i = 1; i < count; i++) {
      cl_assert(mdict_set(m, KEY(i), (int64_t) i, NULL, true));
      
      for (int j = 1; j <= i; j++) {
        cl_assert(mdict_get(m, KEY(j), &v));
        cl_assert_equal_i(v, j);
      }
      for (int j = i+1; j < count; j++) {
        cl_assert(!mdict_get(m, KEY(j), &v));
      }
    }

    for (int i = count; i < 2*count; i++) {
      cl_assert(!mdict_get(m, KEY(i), &v));
    }

    // remove forwards
    for (int i = 1; i < count; i++) {
      cl_assert(mdict_remove(m, KEY(i), &v));
      cl_assert_equal_i(v, i);

      for (int j = 1; j <= i; j++) {
        cl_assert(!mdict_get(m, KEY(j), &v));
      }
      for (int j = i+1; j < count; j++) {
        cl_assert(mdict_get(m, KEY(j), &v));
        cl_assert_equal_i(v, j);
      }
    }

    for (int i = 1; i < count; i++) {
      cl_assert(!mdict_get(m, KEY(i), &v));
    }

    for (int i = 1; i < count; i++) {
      cl_assert(mdict_set(m, KEY(i), (int64_t) i, NULL, true));
    }

    // remove backwards
    for (int i = count - 1; i >= 1; i--) {
      cl_assert(mdict_remove(m, KEY(i), &v));
      cl_assert_equal_i(v, i);

      for (int j = i; j < count; j++) {
        cl_assert(!mdict_get(m, KEY(j), &v));
      }
      for (int j = 1; j < i; j++) {
        cl_assert(mdict_get(m, KEY(j), &v));
        cl_assert_equal_i(v, j);
      }
    }
  }
}
