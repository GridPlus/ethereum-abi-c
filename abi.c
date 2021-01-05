#include "abi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//===============================================
// HELPERS
//===============================================

// Get the u32 that is represented in big endian in a 32 byte word (i.e. last 4 bytes).
// Returns a little endian numerical representation of bytes loc+29:loc+32.
static uint32_t get_abi_u32_be(const void * in, size_t loc) {
  const uint8_t * inPtr = in;
  size_t l = loc + 32;
  return inPtr[l-1] | inPtr[l-2] << 8 | inPtr[l-3] << 16 | inPtr[l-4] << 24;
}

static bool is_fixed_bytes_type(ABI_t t) {
  return t.type >= ABI_BYTES1 && t.type <= ABI_BYTES32;
}

// Dynamic types include `bytes` and `string` - they are always variable length.
static bool is_dynamic_atomic_type(ABI_t t) {
  return (t.type == ABI_BYTES || t.type == ABI_STRING);
}

// Elementary types are atomic types for which there is only one instance (as
// opposed to array types, which contain a fixed or variable number of instances).
static bool is_elementary_atomic_type(ABI_t t) {
  return false == is_dynamic_atomic_type(t);
}

static bool is_single_elementary_type(ABI_t t) {
  return ((true == is_elementary_atomic_type(t)) &&
          (false == t.isArray));
}

static bool is_single_dynamic_type(ABI_t t) {
  return ((true == is_dynamic_atomic_type(t)) &&
          (false == t.isArray));
}

// Array types are simply arrays of elementary types. These can be either 
// fixed size (e.g. uint256[3]) or variable size (e.g. uint256[]). Variable sized
// arrays have `t.isArray = true, t.arraySz = 0` while fixed size have the size
// defined in `t.arraySz`.
static bool is_elementary_type_array(ABI_t t) {
  return ((true == is_elementary_atomic_type(t)) &&
          (true == t.isArray));
}

static bool is_dynamic_type_array(ABI_t t) {
  return ((true == is_dynamic_atomic_type(t)) &&
          (true == t.isArray));
}

static inline bool is_fixed_sz_array(ABI_t type) {
  // The ABI spec doesn't really handle multi-dimensional fixed sized arrays,
  // so we will reject any fixed size arrays beyond 1D.
  // The reference implementation (ethereumjs-abi) treats x[3][3] exactly the
  // same as x[3][] and x[3][1], which we do not want to allow.
  // Array must be 1D and have a non-zero arraySz
  return (type.isArray && type.arraySz > 0);
}

static inline bool is_variable_sz_array(ABI_t type) {
  // arraySz must be 0 to denote variable sized arrays.
  // Although the ABI spec does describe 2D arrays, we do not currently support
  // them as they introduce quite a bit of complexity and aren't used very often.
  return (type.isArray && type.arraySz == 0);
}

static bool is_elementary_type_fixed_sz_array(ABI_t type) {
  return ((is_elementary_type_array(type)) && 
          (is_fixed_sz_array(type)));
}

static bool is_elementary_type_variable_sz_array(ABI_t type) {
  return ((is_elementary_type_array(type)) &&
          (is_variable_sz_array(type)));
}

static bool is_dynamic_type_fixed_sz_array(ABI_t type) {
  return ((is_dynamic_type_array(type)) &&
          (is_fixed_sz_array(type)));
}

static bool is_dynamic_type_variable_sz_array(ABI_t type) {
  return ((is_dynamic_type_array(type)) &&
          (is_variable_sz_array(type)));
}

// Get the number of bytes describing an elementary data type
static size_t elem_sz(ABI_t t) {
  if (true == is_dynamic_atomic_type(t))
    return 0;
  if (true == is_fixed_bytes_type(t))
    return 1 + (t.type - ABI_BYTES1);
  switch (t.type) {
    // Non-numerical
    case ABI_ADDRESS:
      return 20;
    case ABI_BOOL:
      return 1;
    // Numerical
    case ABI_UINT8:
    case ABI_INT8:
      return 1;
    case ABI_UINT16:
    case ABI_INT16:
      return 2;
    case ABI_UINT32:
    case ABI_INT32:
      return 4;
    case ABI_UINT64:
    case ABI_INT64:
      return 8;
    case ABI_UINT128:
    case ABI_INT128:
      return 16;
    case ABI_UINT256:
    case ABI_INT256:
    case ABI_UINT:
    case ABI_INT:
      return 32;
    default:
      return 0;
  }
}

// Decode a parameter of elementary type. Each elementary type is encoded in a single 32 byte word,
// but may contain less data than 32 bytes.
static size_t decode_elem_param(void * out, size_t outSz, ABI_t type, const void * in, size_t inSz, size_t off) {
  // Ensure there is space for this data in `out` and that this is an elementary type
  if ((ABI_WORD_SZ > outSz) || (true == is_dynamic_atomic_type(type)))
    return 0;
  size_t nBytes = elem_sz(type);
  const uint8_t * inPtr = in;
  if (outSz < nBytes)
    return 0;
  // Most types have data written at the end of the word. Start with this assumption. 
  size_t start = off + (ABI_WORD_SZ - nBytes);
  // Non-numerical (and non-bool) types have data written to the beginning of the word
  if (true == is_fixed_bytes_type(type))
    start = off;
  if (start + nBytes > inSz)
    return 0;
  memcpy(out, inPtr + start, nBytes);
  return nBytes;
}

// Decode a parameter of dynamic type. Each dynamic type is encoded as a 32 byte size prefix word, which
// describes the size of the dynamic type, and `N` words which fully capture the dynamic data.
// We only copy the data itself, i.e. we discard the right-padded zeros.
static size_t decode_dynamic_param( void * out, 
                                    size_t outSz, 
                                    ABI_t type, 
                                    const void * in, 
                                    size_t inSz, 
                                    size_t off,
                                    bool szOnly) {
  if (false == is_dynamic_atomic_type(type))
    return 0;
  const uint8_t * inPtr = in;
  if (off + ABI_WORD_SZ > inSz)
    return 0;
  size_t elemSz = get_abi_u32_be(in, off);
  off += ABI_WORD_SZ;
  // A user may only wish to know the *size* of this param (i.e. call `abi_get_param_sz`).
// In this case we bypass the sanity checks and do NOT copy data.
if (szOnly == false) {
if (outSz < elemSz)
return 0;
    if (off + elemSz > inSz)
      return 0;
    memcpy(out, inPtr + off, elemSz);
  }
  return elemSz;
}

// Decode a "non-elementary" param. This includes dynamic types as well as both fixed and vairable
// sized arrays containing a set of params of a single elementary type.
static size_t decode_param( void * out, 
                            size_t outSz, 
                            ABI_t type, 
                            const void * in, 
                            size_t inSz, 
                            size_t off, 
                            ABISelector_t info,
                            bool szOnly) 
{
  size_t numElem = 0;
  if (is_elementary_type_array(type)) {
    // Handle elementary type arrays. Each element in these arrays is of size ABI_WORD_SZ.
    // For fixed size arrays, the number of elements is defined in the ABI itself.
    // For variable size arrays, the number of elements is encoded in the offset word.
    numElem = type.arraySz;
    if (numElem == 0) {
      if (off + ABI_WORD_SZ > inSz)
        return 0;
      numElem = get_abi_u32_be(in, off);
      off += ABI_WORD_SZ; // Account for this word, which tells us the number of ensuing elements.
    }
    // Make sure we don't overflow the buffer
    if (info.arrIdx >= numElem)
      return 0;
    return decode_elem_param(out, outSz, type, in, inSz, off + (ABI_WORD_SZ * info.arrIdx));
  }
  // For dynamic types, the param offset word contains the number of *bytes*, and we can copy them directly.
  // Dynamic arrays have an additional word in front of each element, which describes
  // the number of bytes in that element. Note that these elements are written in 32
  // byte words, but may take multiple words (e.g. a 36 byte array would take 2 words).
  if (true == is_dynamic_type_array(type)) {
    if (true == is_dynamic_type_fixed_sz_array(type)) {
      // Skip elements in this fixed-size array of dynamic elements until we reach
      // the index of the data we want to fetch.
      for (size_t i = 0; i < info.arrIdx; i++) {
        if (off + ABI_WORD_SZ > inSz)
          return 0;
        size_t esz = get_abi_u32_be(in, off);
        off += ABI_WORD_SZ * (1 + (1 + (esz / ABI_WORD_SZ)));
      }
    } else {
      // Get the number of elements in the array of this dimension and bump the offset
      if (off + ABI_WORD_SZ > inSz)
        return 0;
      numElem = get_abi_u32_be(in, off);
      off += ABI_WORD_SZ;
      // Make sure we don't overflow the buffer
      if (info.arrIdx >= numElem)
        return 0;
      // Skip past the variable sized elements that come before the one we want to fetch.
      for (size_t i = 0; i < info.arrIdx; i++) {
        if (off + ABI_WORD_SZ > inSz)
          return 0;
        size_t nestedArrSz = get_abi_u32_be(in, off);
        size_t numWords = 1 + (nestedArrSz / ABI_WORD_SZ);
        // Skip the size word and all words containing this element
        off += (1 + numWords) * ABI_WORD_SZ; 
      }
    }
  }
  // We should now be at the offset corresponding to the size of the dynamic
  // type element that we want.
  return decode_dynamic_param(out, outSz, type, in, inSz, off, szOnly);
}

// Account for data that is not included in the word offset. We term this "extra data".
// This is pretty confusing, so please see example 7 in test.c. The 7-th word is 0x80, which is the
// offset of the `uint[]` data. This is 128 / 32 = 4 words. This can be interpreted as skipping over
// 3 strings (one word each in the case of ex7) and the offset word (0x80). However, if you look at the 
// payload, the data itself is actually at an offset of 8 words. This is because the `string[3]` has 3 words
// each describing a size of a dynamic element (there are 3). For some reason, the Ethereum ABI protocol
// does *not* count these three "size words" in the offset. If this were instead a `uint[3]` (see: example 10), 
// the offset would be the normal 4 words because those 3 elements are fixed size and do not need size descriptors
// prefixing each element. Anyway, the weird part is that the size descriptors do not count towards offsets
// for fixed-size, dynamic-type arrays.
static size_t get_dynamic_extra_data_sz(const ABI_t * types, size_t nTypes, const void * in, size_t inSz) {
  size_t off = 0;
  for (size_t i = 0; i < nTypes; i++) {
    if (true == is_dynamic_type_fixed_sz_array(types[i])) {
      // If there is a dynamic type fixed size array after the target type, we need to
      // get the size of the data, since it doesn't get counted in the offset.
      ABISelector_t info = {
        .typeIdx = i,
        .arrIdx = 0,
      };
      for (size_t j = 0; j < types[i].arraySz; j++) {
        info.arrIdx = j;
        size_t paramSz = abi_get_param_sz(types, nTypes, info, in, inSz);
        off += ABI_WORD_SZ * (1 + (paramSz / ABI_WORD_SZ));
      }
    }
  }
  return off;
}

// Get the offset of a param in the payload. This accounts for the fixed array dynamic type
// edge case and returns the offset at which the `idx`-th param begins.
static size_t get_param_offset(const ABI_t * types, size_t idx, const void * in, size_t inSz) {
  size_t off = 0;
  for (size_t i = 0; i < idx; i++) {
    if (true == is_dynamic_type_fixed_sz_array(types[i])) {
      for (size_t j = 0; j < types[i].arraySz; j++) {
        // Offset for word describing item size and the item itself, which 
        // may be more than one word.
        if (off + ABI_WORD_SZ > inSz)
          return 0;
        off += ABI_WORD_SZ * (2 + (get_abi_u32_be(in, off) / ABI_WORD_SZ));
      }
    } else if (true == is_elementary_type_fixed_sz_array(types[i])) {
      off += ABI_WORD_SZ * types[i].arraySz;
    } else {
      off += ABI_WORD_SZ;
    }
  }
  return off;
}

// If we have a single dynamic type or an array type that has not yet been accounted for,
// the type's data will have an offset that may require correction if there are fixed-size,
// dynamic-type arrays elsewhere in the function.
// This function returns the extra offset that needs to be added in order to find this type's data.
static size_t get_extra_dynamic_offset(const ABI_t * types, size_t numTypes, const void * in, size_t inSz, ABI_t type) {
    return get_dynamic_extra_data_sz(types, numTypes, in, inSz);
}

//===============================================
// API
//===============================================
bool abi_is_valid_schema(const ABI_t * types, size_t numTypes) {
  if (types == NULL || numTypes == 0)
    return false;
  for (size_t i = 0; i < numTypes; i++) {
    if ((types[i].type >= ABI_MAX || types[i].type <= ABI_NONE) ||
        ( (false == is_single_elementary_type(types[i])) &&
          (false == is_single_dynamic_type(types[i])) &&
          (false == is_elementary_type_fixed_sz_array(types[i])) &&
          (false == is_elementary_type_variable_sz_array(types[i])) &&
          (false == is_dynamic_type_fixed_sz_array(types[i])) &&
          (false == is_dynamic_type_variable_sz_array(types[i])) ))
      return false;
  }
  return true;
}

size_t abi_get_array_sz(const ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        const void * in,
                        size_t inSz)
{
  ABI_t type = types[info.typeIdx];
  if ((types == NULL || in == NULL) ||
      (info.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)) ||
      (false == is_variable_sz_array(type)) )
    return 0;
  // Get the offset of this parameter in the function signature
  size_t wordOff = get_param_offset(types, info.typeIdx, in, inSz);
  // For a variable size array (which this must be), the `wordOff`
  // word contains an offset pointing to where the array data begins.
  uint32_t off = get_abi_u32_be(in, wordOff);
  // Account for data offset edge cases
  off += get_extra_dynamic_offset(types, numTypes, in, inSz, type);
  // Get the size of the dimension we want
  return get_abi_u32_be(in, off);
}

size_t abi_get_param_sz(const ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        const void * in, 
                        size_t inSz)
{
  ABI_t type = types[info.typeIdx];
  if ((types == NULL || in == NULL) ||
      (info.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)) ||
      (false == is_dynamic_atomic_type(type)))
    return 0;

  // We will build a fake output buffer to reuse the `decode_param` function.
  uint8_t out[1] = {0};
  uint8_t outSz = 1;
  // Since we know this is a dynamic type, we only have two paths here, which
  // have been taken from `abi_decode_param`.
  size_t wordOff = get_param_offset(types, info.typeIdx, in, inSz);
  if (true == is_dynamic_type_fixed_sz_array(types[info.typeIdx]))
    return decode_param(out, outSz, type, in, inSz, wordOff, info, true);
  uint32_t off = get_abi_u32_be(in, wordOff);
  // Account for data offset edge cases
  off += get_extra_dynamic_offset(types, numTypes, in, inSz, type);
  // The next word describes the length of the array in question.
  return decode_param(out, outSz, type, in, inSz, off, info, true);
}

size_t abi_decode_param(void * out, 
                        size_t outSz, 
                        const ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        const void * in,
                        size_t inSz) 
{
  if (out == NULL || types == NULL || in == NULL)
    return 0;

  // Ensure we have valid types passed
  ABI_t type = types[info.typeIdx];
  if ((info.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)))
    return 0;

  // Offset of the word in the encoded blob. Per ABI spec, the first `n` words correspond
  // to the function params. These words contain parameter data if the parameter in question
  // is of elementary type. If it is a dynamic type param, the word contains information about
  // the offset containing the raw data.
  // size_t wordOff = ABI_WORD_SZ * info.typeIdx;
  size_t wordOff = get_param_offset(types, info.typeIdx, in, inSz);

  // ELEMENTARY TYPES:
  // Elementary types are all 32 bytes long and can be easily memcopied into our output buffer.
  if (true == is_single_elementary_type(type)) {
    // For single value elementary params, the param offset is all we need
    return decode_elem_param(out, outSz, type, in, inSz, wordOff);
  } else if (true == is_elementary_type_fixed_sz_array(type)) {
    if (type.arraySz <= info.arrIdx)
      return 0;
    // For fixed size arrays, we need to add skipped elements to the offset
    wordOff += ABI_WORD_SZ * info.arrIdx;
    return decode_elem_param(out, outSz, type, in, inSz, wordOff);
  }

  // FIXED-ARRAY DYNAMIC TYPES:
  // For arrays of fixed size which contain dynamic type params, we just need to skip to the
  // param offset (i.e. the data is *not* offset as it is for variable-length arrays)
  if (true == is_dynamic_type_fixed_sz_array(type)) {
    if (type.arraySz <= info.arrIdx)
      return 0;
    return decode_param(out, outSz, type, in, inSz, wordOff, info, false);
  }

  // SINGLE DYNAMIC AND OTHER ARRAY TYPES:
  // If we have made it here, we need to account for adjustments in the offset.
  // See `get_extra_dynamic_offset` for more info.

  // The value at the `typeIdx`-th word in the input buffer contains the offset of the data (in BE).
  // We assume the index can be captured in a u32, so we only inspect the last 4 bytes
  // of the 32 byte word. We cannot realistically have payloads longer than a few kB at
  // the absolute max, so I don't see any way an offset could be larger than U32_MAX.
  uint32_t off = get_abi_u32_be(in, wordOff);
  off += get_extra_dynamic_offset(types, numTypes, in, inSz, type);

  // Decode the param
  return decode_param(out, outSz, type, in, inSz, off, info, false);
}