#include "abi.h"
#include "test_vec.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])

//===============================================================
// TESTS
//===============================================================

// static void print_data(uint8_t * in, size_t sz) {
//   for (size_t i = 0; i < sz; i++) {
//     printf("0x%x, ", in[i]);
//   }
//   printf("\n\r");
// }

static inline uint32_t get_u32_be(uint8_t * in, size_t off) {
  return (in[off + 3] | in[off + 2] << 8 | in[off + 1] << 16 | in[off + 0] << 24);
}

static inline void test_ex1(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex1_encoded+4;
  size_t inSz = sizeof(ex1_encoded) - 4;
  printf("Example 1...");
  // function baz(uint32 x, bool y)
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) == sizeof(ex1_param_0));
  assert((out[3] | out[2] << 8 | out[1] << 16 | out[0] << 24) == ex1_param_0);
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) == sizeof(ex1_param_1));
  assert((bool) out[0] == ex1_param_1);
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex2(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex2_encoded+4;
  size_t inSz = sizeof(ex2_encoded) - 4;
  printf("Example 2...");
  // function bar(bytes3[2])
  info.typeIdx = 0;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex2_abi, ARRAY_SIZE(ex2_abi), info, in, inSz) == sizeof(ex2_param_00));
  assert(0 == memcmp(ex2_param_00, out, sizeof(ex2_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex2_abi, ARRAY_SIZE(ex2_abi), info, in, inSz) == sizeof(ex2_param_01));
  assert(0 == memcmp(ex2_param_01, out, sizeof(ex2_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 0;
  printf("passed.\n\r");
}

static inline void test_ex3(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex3_encoded+4;
  size_t inSz = sizeof(ex3_encoded) - 4;
  printf("Example 3...");
  // function sam(bytes, bool, uint[])
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_0)); // 4 bytes in payload
  assert(0 == memcmp(ex3_param_0, out, sizeof(ex3_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_1)); // 1 byte for bool
  assert((bool) out[0] == ex3_param_1);
  memset(out, 0, outSz);
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_20)); // 3 uint values, each 32 bytes
  assert(0 == memcmp(ex3_param_20, out, sizeof(ex3_param_20)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_21)); // 3 uint values, each 32 bytes
  assert(0 == memcmp(ex3_param_21, out, sizeof(ex3_param_21)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_22)); // 3 uint values, each 32 bytes
  assert(0 == memcmp(ex3_param_22, out, sizeof(ex3_param_22)));
  memset(out, 0, outSz);
  info.arrIdx = 0;

  // Validate variable array size
  info.typeIdx = 2;
  assert(abi_get_array_sz(ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == 3);

  printf("passed.\n\r");
}

static inline void test_ex4(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex4_encoded+4;
  size_t inSz = sizeof(ex4_encoded) - 4;
  printf("Example 4...");
  // f(uint,uint32[],bytes10,bytes)
  info.typeIdx = 0;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_0)); // uint = uint256 = 32 bytes
  assert(0 == memcmp(ex4_param_0, out, sizeof(ex4_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_10)); // uint32 (see payload)
  assert(get_u32_be(out, 0) == ex4_param_10);
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_11)); // uint32 (see payload)
  assert(get_u32_be(out, 0) == ex4_param_11);
  memset(out, 0, outSz);
  info.arrIdx =  0;
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_2)); // bytes10 = 10 bytes
  assert(0 == memcmp(ex4_param_2, out, sizeof(ex4_param_2)));
  memset(out, 0, outSz);
  info.typeIdx = 3;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_3)); // 0x0d = 13 bytes
  assert(0 == memcmp(ex4_param_3, out, sizeof(ex4_param_3)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == 2);

  printf("passed.\n\r");
}

static inline void test_ex5(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex5_encoded+4;
  size_t inSz = sizeof(ex5_encoded) - 4;
  printf("Example 5...");
  // f(uint[3],uint[])
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_00));
  assert(0 == memcmp(ex5_param_00, out, sizeof(ex5_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_01));
  assert(0 == memcmp(ex5_param_01, out, sizeof(ex5_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_02));
  assert(0 == memcmp(ex5_param_02, out, sizeof(ex5_param_02)));
  memset(out, 0, outSz);

  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_10));
  assert(0 == memcmp(ex5_param_10, out, sizeof(ex5_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_11));
  assert(0 == memcmp(ex5_param_11, out, sizeof(ex5_param_11)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == 2);

  printf("passed.\n\r");
}

static inline void test_ex6(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex6_encoded;
  size_t inSz = sizeof(ex6_encoded);
  printf("Example 6...");
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_00));
  assert(0 == memcmp(ex6_param_00, out, sizeof(ex6_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_01));
  assert(0 == memcmp(ex6_param_01, out, sizeof(ex6_param_01)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_10));
  assert(0 == memcmp(ex6_param_10, out, sizeof(ex6_param_10)));
  memset(out, 0, outSz);  
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_11));
  assert(0 == memcmp(ex6_param_11, out, sizeof(ex6_param_11)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_20));
  assert(0 == memcmp(ex6_param_20, out, sizeof(ex6_param_20)));
  memset(out, 0, outSz);

  // Validate param sizes
  info.typeIdx = 0;
  info.arrIdx = 0;
  assert(abi_get_param_sz(ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == ARRAY_SIZE(ex6_param_00));
  info.arrIdx = 1;
  assert(abi_get_param_sz(ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == ARRAY_SIZE(ex6_param_01));
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_get_param_sz(ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == ARRAY_SIZE(ex6_param_20));

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == 3);

  printf("passed.\n\r");
}

static inline void test_ex7(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex7_encoded;
  size_t inSz = sizeof(ex7_encoded);
  printf("Example 7...");
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_00));
  assert(0 == memcmp(ex7_param_00, out, sizeof(ex7_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_01));
  assert(0 == memcmp(ex7_param_01, out, sizeof(ex7_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_02));
  assert(0 == memcmp(ex7_param_02, out, sizeof(ex7_param_02)));
  memset(out, 0, outSz);

  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_10));
  assert(0 == memcmp(ex7_param_10, out, sizeof(ex7_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_11));
  assert(0 == memcmp(ex7_param_11, out, sizeof(ex7_param_11)));
  memset(out, 0, outSz);

  // Validate param sizes
  info.typeIdx = 0;
  info.arrIdx = 0;
  assert(abi_get_param_sz(ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == ARRAY_SIZE(ex7_param_00));
  info.arrIdx = 1;
  assert(abi_get_param_sz(ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == ARRAY_SIZE(ex7_param_01));
  info.arrIdx = 2;
  assert(abi_get_param_sz(ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == ARRAY_SIZE(ex7_param_02));

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == 2);
  printf("passed.\n\r");
}

static inline void test_ex8(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex8_encoded;
  size_t inSz = sizeof(ex8_encoded);
  printf("Example 8...");
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_00));
  assert(0 == memcmp(ex8_param_00, out, sizeof(ex8_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_01));
  assert(0 == memcmp(ex8_param_01, out, sizeof(ex8_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_02));
  assert(0 == memcmp(ex8_param_02, out, sizeof(ex8_param_02)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_10));
  assert(0 == memcmp(ex8_param_10, out, sizeof(ex8_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_11));
  assert(0 == memcmp(ex8_param_11, out, sizeof(ex8_param_11)));
  memset(out, 0, outSz);

  // Validate param sizes
  info.typeIdx = 0;
  info.arrIdx = 0;
  assert(abi_get_param_sz(ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == ARRAY_SIZE(ex8_param_00));
  info.arrIdx = 1;
  assert(abi_get_param_sz(ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == ARRAY_SIZE(ex8_param_01));
  info.arrIdx = 2;
  assert(abi_get_param_sz(ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == ARRAY_SIZE(ex8_param_02));

  // Validate array size
  assert(abi_get_array_sz(ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == 3);

  printf("passed.\n\r");
}

static inline void test_ex9(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex9_encoded;
  size_t inSz = sizeof(ex9_encoded);
  printf("Example 9...");

  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_0));
  assert(0 == memcmp(ex9_param_0, out, sizeof(ex9_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_10));
  assert(0 == memcmp(ex9_param_10, out, sizeof(ex9_param_10)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_20));
  assert(0 == memcmp(ex9_param_20, out, sizeof(ex9_param_20)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_21));
  assert(0 == memcmp(ex9_param_21, out, sizeof(ex9_param_21)));
  memset(out, 0, outSz);
  info.typeIdx = 3;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_3));
  assert(0 == memcmp(ex9_param_3, out, sizeof(ex9_param_3)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex10(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex10_encoded;
  size_t inSz = sizeof(ex10_encoded);
  printf("Example 10...");
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_00));
  assert(0 == memcmp(ex10_param_00, out, sizeof(ex10_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_01));
  assert(0 == memcmp(ex10_param_01, out, sizeof(ex10_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_02));
  assert(0 == memcmp(ex10_param_02, out, sizeof(ex10_param_02)));
  memset(out, 0, outSz);

  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_10));
  assert(0 == memcmp(ex10_param_10, out, sizeof(ex10_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_11));
  assert(0 == memcmp(ex10_param_11, out, sizeof(ex10_param_11)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex11(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex11_encoded;
  size_t inSz = sizeof(ex11_encoded);
  printf("Example 11...");
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_0));
  assert(0 == memcmp(ex11_param_0, out, sizeof(ex11_param_0)));
  assert(abi_get_param_sz(ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == ARRAY_SIZE(ex11_param_0));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_10));
  assert(0 == memcmp(ex11_param_10, out, sizeof(ex11_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_11));
  assert(0 == memcmp(ex11_param_11, out, sizeof(ex11_param_11)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_12));
  assert(0 == memcmp(ex11_param_12, out, sizeof(ex11_param_12)));
  memset(out, 0, outSz);
  info.arrIdx = 3;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_13));
  assert(0 == memcmp(ex11_param_13, out, sizeof(ex11_param_13)));
  memset(out, 0, outSz);
  info.arrIdx = 4;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_14));
  assert(0 == memcmp(ex11_param_14, out, sizeof(ex11_param_14)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_2));
  assert(0 == memcmp(ex11_param_2, out, sizeof(ex11_param_2)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex12(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex12_encoded;
  size_t inSz = sizeof(ex12_encoded);
  printf("Example 12...");
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_00));
  assert(0 == memcmp(ex12_param_00, out, sizeof(ex12_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_01));
  assert(0 == memcmp(ex12_param_01, out, sizeof(ex12_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_02));
  assert(0 == memcmp(ex12_param_02, out, sizeof(ex12_param_02)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_1));
  assert(0 == memcmp(ex12_param_1, out, sizeof(ex12_param_1)));
  assert(abi_get_param_sz(ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == ARRAY_SIZE(ex12_param_1));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_2));
  assert(get_u32_be(out, 0) == ex12_param_2);
  memset(out, 0, outSz);
  info.typeIdx = 3;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_30));
  assert(0 == memcmp(ex12_param_30, out, sizeof(ex12_param_30)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_31));
  assert(0 == memcmp(ex12_param_31, out, sizeof(ex12_param_31)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex13(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex13_encoded;
  size_t inSz = sizeof(ex13_encoded);
  printf("Example 13...");
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_00));
  assert(0 == memcmp(ex13_param_00, out, sizeof(ex13_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_01));
  assert(0 == memcmp(ex13_param_01, out, sizeof(ex13_param_01)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_10));
  assert(0 == memcmp(ex13_param_10, out, sizeof(ex13_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_11));
  assert(0 == memcmp(ex13_param_11, out, sizeof(ex13_param_11)));
  assert(abi_get_param_sz(ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == ARRAY_SIZE(ex13_param_11));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_failures(uint8_t * out, size_t outSz) {
  // Confirm bad schemas are rejected
  ABI_t fail_ex1_abi[1] = {
    { .type = 103820, },
  };
  assert(false == abi_is_valid_schema(fail_ex1_abi, ARRAY_SIZE(fail_ex1_abi)));
  // Test shorter `inSz` values
  uint8_t * in = ex1_encoded+4;
  size_t inSz = sizeof(ex1_encoded) - 4;
  ABISelector_t info = { .typeIdx = 0 };
  // We need inSz to be at least 32 bytes to allow for extracting of the first word
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, ABI_WORD_SZ) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, ABI_WORD_SZ-1) == 0);
  // We need inSz to be at least 64 bytes to allow for extracting of the second word
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, 2*ABI_WORD_SZ) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, 2*ABI_WORD_SZ-1) == 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz-1) == 0);
  memset(out, 0, outSz);
  // Make sure we cannot specify an index out of range of a fixed size array
  in = ex5_encoded+4;
  inSz = sizeof(ex5_encoded) - 4;
  info.typeIdx = 0;
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) > 0);
  info.arrIdx = 3;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == 0);
}

int main() {
  printf("=============================\n\r");
  printf(" RUNNING ABI TESTS...\n\r");
  printf("=============================\n\r");
  uint8_t out[200] = {0};
  test_ex1(out, sizeof(out));
  test_ex2(out, sizeof(out));
  test_ex3(out, sizeof(out));
  test_ex4(out, sizeof(out));
  test_ex5(out, sizeof(out));
  test_ex6(out, sizeof(out));
  test_ex7(out, sizeof(out));
  test_ex8(out, sizeof(out));
  test_ex9(out, sizeof(out));
  test_ex10(out, sizeof(out));
  test_ex11(out, sizeof(out));
  test_ex12(out, sizeof(out));
  test_ex13(out, sizeof(out));
  test_failures(out, sizeof(out));

  printf("=============================\n\r");
  printf(" ALL ABI TESTS PASSING!\n\r");
  printf("=============================\n\r");
  
  return 0;
}
