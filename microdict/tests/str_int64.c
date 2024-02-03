#include "clar.h"
#include <stdint.h>
#include <stdio.h>

#include "../flags.h"
#define KEY_TYPE_TAG TYPE_TAG_STR
#define VAL_TYPE_TAG TYPE_TAG_I64
#include "../abstract.h"

static h_t* m;
static char* k_search;
static char* k_ins;

void test_str_int64__initialize(void) {
  m = mdict_create(32, true);
  k_search = malloc(4);
  k_ins = malloc(4);
}

void test_str_int64__cleanup(void) {
  free(k_ins);
  free(k_search);
  mdict_destroy(m);
}

void test_str_int64__lots_of_insertions(void) {
  v_t v;
  for (int loop = 0; loop < 10; loop++) {
    cl_assert(m->size == 0);

    int count = 201;
    for (int i = 1; i < count; i++) {
      sprintf(k_ins, "%d", i);
      cl_assert(mdict_set(m, k_ins, (int64_t) i, NULL, true));
      
      for (int j = 1; j <= i; j++) {
        sprintf(k_search, "%d", j);
        cl_assert(mdict_get(m, k_search, &v));
        cl_assert_equal_i(v, j);
      }
      for (int j = i+1; j < count; j++) {
        sprintf(k_search, "%d", j);
        cl_assert(!mdict_get(m, k_search, &v));
      }
    }

    for (int i = count; i < 2*count; i++) {
      sprintf(k_search, "%d", i);
      cl_assert(!mdict_get(m, k_search, &v));
    }

    // remove forwards
    for (int i = 1; i < count; i++) {
      sprintf(k_search, "%d", i);
      cl_assert(mdict_remove(m, k_search, &v));
      cl_assert_equal_i(v, i);

      for (int j = 1; j <= i; j++) {
        sprintf(k_search, "%d", j);
        cl_assert(!mdict_get(m, k_search, &v));
      }
      for (int j = i+1; j < count; j++) {
        sprintf(k_search, "%d", j);
        cl_assert(mdict_get(m, k_search, &v));
        cl_assert_equal_i(v, j);
      }
    }

    for (int i = 1; i < count; i++) {
      sprintf(k_search, "%d", i);
      cl_assert(!mdict_get(m, k_search, &v));
    }

    for (int i = 1; i < count; i++) {
      sprintf(k_ins, "%d", i);
      cl_assert(mdict_set(m, k_ins, (int64_t) i, NULL, true));
    }

    // remove backwards
    for (int i = count - 1; i >= 1; i--) {
      sprintf(k_search, "%d", i);
      cl_assert(mdict_remove(m, k_search, &v));
      cl_assert_equal_i(v, i);

      for (int j = i; j < count; j++) {
        sprintf(k_search, "%d", j);
        cl_assert(!mdict_get(m, k_search, &v));
      }
      for (int j = 1; j < i; j++) {
        sprintf(k_search, "%d", j);
        cl_assert(mdict_get(m, k_search, &v));
        cl_assert_equal_i(v, j);
      }
    }
  }
}
