/*
 *
 * Copyright 2014, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "src/core/transport/metadata.h"

#include <stdio.h>

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include "test/core/util/test_config.h"

#define LOG_TEST() gpr_log(GPR_INFO, "%s", __FUNCTION__)

/* a large number */
#define MANY 1000000

static void test_no_op() {
  grpc_mdctx *ctx;

  LOG_TEST();

  ctx = grpc_mdctx_create();
  grpc_mdctx_orphan(ctx);
}

static void test_create_string() {
  grpc_mdctx *ctx;
  grpc_mdstr *s1, *s2, *s3;

  LOG_TEST();

  ctx = grpc_mdctx_create();
  s1 = grpc_mdstr_from_string(ctx, "hello");
  s2 = grpc_mdstr_from_string(ctx, "hello");
  s3 = grpc_mdstr_from_string(ctx, "very much not hello");
  GPR_ASSERT(s1 == s2);
  GPR_ASSERT(s3 != s1);
  GPR_ASSERT(gpr_slice_str_cmp(s1->slice, "hello") == 0);
  GPR_ASSERT(gpr_slice_str_cmp(s3->slice, "very much not hello") == 0);
  grpc_mdstr_unref(s1);
  grpc_mdstr_unref(s2);
  grpc_mdctx_orphan(ctx);
  grpc_mdstr_unref(s3);
}

static void test_create_metadata() {
  grpc_mdctx *ctx;
  grpc_mdelem *m1, *m2, *m3;

  LOG_TEST();

  ctx = grpc_mdctx_create();
  m1 = grpc_mdelem_from_strings(ctx, "a", "b");
  m2 = grpc_mdelem_from_strings(ctx, "a", "b");
  m3 = grpc_mdelem_from_strings(ctx, "a", "c");
  GPR_ASSERT(m1 == m2);
  GPR_ASSERT(m3 != m1);
  GPR_ASSERT(m3->key == m1->key);
  GPR_ASSERT(m3->value != m1->value);
  GPR_ASSERT(gpr_slice_str_cmp(m1->key->slice, "a") == 0);
  GPR_ASSERT(gpr_slice_str_cmp(m1->value->slice, "b") == 0);
  GPR_ASSERT(gpr_slice_str_cmp(m3->value->slice, "c") == 0);
  grpc_mdelem_unref(m1);
  grpc_mdelem_unref(m2);
  grpc_mdelem_unref(m3);
  grpc_mdctx_orphan(ctx);
}

static void test_create_many_ephemeral_metadata() {
  grpc_mdctx *ctx;
  char buffer[256];
  long i;
  size_t mdtab_capacity_before;

  LOG_TEST();

  ctx = grpc_mdctx_create();
  mdtab_capacity_before = grpc_mdctx_get_mdtab_capacity_test_only(ctx);
  /* add, and immediately delete a bunch of different elements */
  for (i = 0; i < MANY; i++) {
    sprintf(buffer, "%ld", i);
    grpc_mdelem_unref(grpc_mdelem_from_strings(ctx, "a", buffer));
  }
  /* capacity should not grow */
  GPR_ASSERT(mdtab_capacity_before ==
             grpc_mdctx_get_mdtab_capacity_test_only(ctx));
  grpc_mdctx_orphan(ctx);
}

static void test_create_many_persistant_metadata() {
  grpc_mdctx *ctx;
  char buffer[256];
  long i;
  grpc_mdelem **created = gpr_malloc(sizeof(grpc_mdelem *) * MANY);
  grpc_mdelem *md;

  LOG_TEST();

  ctx = grpc_mdctx_create();
  /* add phase */
  for (i = 0; i < MANY; i++) {
    sprintf(buffer, "%ld", i);
    created[i] = grpc_mdelem_from_strings(ctx, "a", buffer);
  }
  /* verify phase */
  for (i = 0; i < MANY; i++) {
    sprintf(buffer, "%ld", i);
    md = grpc_mdelem_from_strings(ctx, "a", buffer);
    GPR_ASSERT(md == created[i]);
    grpc_mdelem_unref(md);
  }
  /* cleanup phase */
  for (i = 0; i < MANY; i++) {
    grpc_mdelem_unref(created[i]);
  }
  grpc_mdctx_orphan(ctx);

  gpr_free(created);
}

static void test_spin_creating_the_same_thing() {
  grpc_mdctx *ctx;

  LOG_TEST();

  ctx = grpc_mdctx_create();
  GPR_ASSERT(grpc_mdctx_get_mdtab_count_test_only(ctx) == 0);
  GPR_ASSERT(grpc_mdctx_get_mdtab_free_test_only(ctx) == 0);

  grpc_mdelem_unref(grpc_mdelem_from_strings(ctx, "a", "b"));
  GPR_ASSERT(grpc_mdctx_get_mdtab_count_test_only(ctx) == 1);
  GPR_ASSERT(grpc_mdctx_get_mdtab_free_test_only(ctx) == 1);

  grpc_mdelem_unref(grpc_mdelem_from_strings(ctx, "a", "b"));
  GPR_ASSERT(grpc_mdctx_get_mdtab_count_test_only(ctx) == 1);
  GPR_ASSERT(grpc_mdctx_get_mdtab_free_test_only(ctx) == 1);

  grpc_mdelem_unref(grpc_mdelem_from_strings(ctx, "a", "b"));
  GPR_ASSERT(grpc_mdctx_get_mdtab_count_test_only(ctx) == 1);
  GPR_ASSERT(grpc_mdctx_get_mdtab_free_test_only(ctx) == 1);

  grpc_mdctx_orphan(ctx);
}

static void test_things_stick_around() {
  grpc_mdctx *ctx;
  int i, j;
  char buffer[64];
  int nstrs = 10000;
  grpc_mdstr **strs = gpr_malloc(sizeof(grpc_mdstr *) * nstrs);
  int *shuf = gpr_malloc(sizeof(int) * nstrs);
  grpc_mdstr *test;

  LOG_TEST();

  ctx = grpc_mdctx_create();

  for (i = 0; i < nstrs; i++) {
    sprintf(buffer, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx%dx", i);
    strs[i] = grpc_mdstr_from_string(ctx, buffer);
    shuf[i] = i;
  }

  for (i = 0; i < nstrs; i++) {
    grpc_mdstr_ref(strs[i]);
    grpc_mdstr_unref(strs[i]);
  }

  for (i = 0; i < nstrs; i++) {
    int p = rand() % nstrs;
    int q = rand() % nstrs;
    int temp = shuf[p];
    shuf[p] = shuf[q];
    shuf[q] = temp;
  }

  for (i = 0; i < nstrs; i++) {
    grpc_mdstr_unref(strs[shuf[i]]);
    for (j = i + 1; j < nstrs; j++) {
      sprintf(buffer, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx%dx", shuf[j]);
      test = grpc_mdstr_from_string(ctx, buffer);
      GPR_ASSERT(test == strs[shuf[j]]);
      grpc_mdstr_unref(test);
    }
  }

  grpc_mdctx_orphan(ctx);
  gpr_free(strs);
  gpr_free(shuf);
}

static void test_slices_work() {
  /* ensure no memory leaks when switching representation from mdstr to slice */
  grpc_mdctx *ctx;
  grpc_mdstr *str;
  gpr_slice slice;

  LOG_TEST();

  ctx = grpc_mdctx_create();

  str = grpc_mdstr_from_string(
      ctx, "123456789012345678901234567890123456789012345678901234567890");
  slice = gpr_slice_ref(str->slice);
  grpc_mdstr_unref(str);
  gpr_slice_unref(slice);

  str = grpc_mdstr_from_string(
      ctx, "123456789012345678901234567890123456789012345678901234567890");
  slice = gpr_slice_ref(str->slice);
  gpr_slice_unref(slice);
  grpc_mdstr_unref(str);

  grpc_mdctx_orphan(ctx);
}

int main(int argc, char **argv) {
  grpc_test_init(argc, argv);
  test_no_op();
  test_create_string();
  test_create_metadata();
  test_create_many_ephemeral_metadata();
  test_create_many_persistant_metadata();
  test_spin_creating_the_same_thing();
  test_things_stick_around();
  test_slices_work();
  return 0;
}