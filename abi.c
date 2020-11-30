#include "abi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#define ABI_WORD_SZ 32

//===============================================
// HELPERS
//===============================================

// Get the u32 that is represented in big endian in a 32 byte word (i.e. last 4 bytes).
// Returns a little endian numerical representation of bytes loc+29:loc+32.
static uint32_t get_abi_u32_be(void * in, size_t loc) {
  uint32_t n = 0;
  for (size_t i = 0; i < sizeof(uint32_t); i++)
    memcpy(&n+i, (in + loc + (31-i)), 1);
  return n;
}

static bool is_valid_abi_type(ABI_t t) {
  return t.type < ABI_MAX && t.type > ABI_NONE;
}

// Dynamic types include `bytes` and `string` - they are always variable length.
static bool is_dynamic_abi_type(ABI_t t) {
  return ((true == is_valid_abi_type(t)) && (
          (t.type == ABI_BYTES || t.type == ABI_STRING)));
}

// Elementary types are atomic types for which there is only one instance (as
// opposed to array types, which contain a fixed or variable number of instances).
static bool is_elementary_abi_type(ABI_t t) {
  return ((true == is_valid_abi_type(t) &&
          (false == is_dynamic_abi_type(t)) &&
          (false == t.isArray)));
}

// Array types are simply arrays of elementary types. These can be either 
// fixed size (e.g. uint256[3]) or variable size (e.g. uint256[]). Variable sized
// arrays have `t.isArray = true, t.arraySz = 0` while fixed size have the size
// defined in `t.arraySz`.
static bool is_array_abi_type(ABI_t t) {
  return ((true == is_valid_abi_type(t) &&
          (false == is_dynamic_abi_type(t)) &&
          (true == t.isArray)));
}

static size_t decode_non_elem_param(void * out, size_t outSz, ABI_t type, void * in, size_t off) {
  if (false == is_valid_abi_type(type))
    return 0;
  size_t arraySzBytes = 0;
  if (is_array_abi_type(type)) {
    // Start by assuming we have a fixed size array, which has the number of elements defined at
    // the param offset word (i.e. the *data* offset starts with the data itself, rather than a size).
    arraySzBytes = type.arraySz * ABI_WORD_SZ;
    if (type.arraySz == 0) {
      // For variable sized arrays, the first word at the data offset location contains the number of
      // array elements; again, this size is described in the *param* offset location for fixed size arrays.
      arraySzBytes = get_abi_u32_be(in, off) * ABI_WORD_SZ;
      off += ABI_WORD_SZ;
    }
  } else {
    // For dynamic types, the param offset word contains the number of *bytes*
    arraySzBytes = get_abi_u32_be(in, off);
  }
  // Ensure we can fit this param data into the `out` buffer. If we can, copy and return.
  if (outSz < arraySzBytes)
    return 0;
  memcpy(out, in+off, arraySzBytes);
  return arraySzBytes;
}

// Decode a parameter of elementary type, which are all 32 bytes at the offset (`off`)
static size_t decode_elem_param(void * out, size_t outSz, ABI_t type, void * in, size_t off) {
  // Ensure there is space for this data in `out` and that this is an elementary type
  if ((off + ABI_WORD_SZ > outSz) || (false == is_valid_abi_type(type)) ||
      (true == is_dynamic_abi_type(type) || true == type.isArray))
    return 0;
  memcpy(in+off, out, ABI_WORD_SZ);
  return ABI_WORD_SZ;
}


//===============================================
// API
//===============================================

size_t abi_decode_param(void * out, size_t outSz, ABI_t * types, size_t typeIdx, void * in) {
  if (out == NULL || types == NULL || in == NULL)
    return 0;

  // Ensure we have valid types passed
  ABI_t type = types[typeIdx];
  if (false == is_valid_abi_type(type))
    return 0;
  
  // Offset of the word in the encoded blob. Per ABI spec, the first `n` words correspond
  // to the function params. These words contain parameter data if the parameter in question
  // is of elementary type. If it is a dynamic type param, the word contains information about
  // the offset containing the raw data.
  size_t wordOff = ABI_WORD_SZ * typeIdx;

  // Elementary types are all 32 bytes long and can be easily memcopied into our output buffer.
  if (true == is_elementary_abi_type(type))
    return decode_elem_param(out, outSz, type, in, wordOff);

  // Dynamic and array types are encoded essentially the same as one another.
  // The value at the `typeIdx`-th word in the input buffer contains the offset of the data (in BE).
  // We assume the index can be captured in a u32, so we only inspect the last 4 bytes
  // of the 32 byte word. We cannot realistically have payloads longer than a few kB at
  // the absolute max, so I don't see any way an offset could be larger than U32_MAX.
  uint32_t off = get_abi_u32_be(in, wordOff);
  // Decode the param
  return decode_non_elem_param(out, outSz, type, in, off);
}