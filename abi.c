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
static uint32_t get_abi_u32_be(void * in, size_t loc) {
  uint8_t * inPtr = in;
  size_t l = loc + 32;
  return inPtr[l-1] | inPtr[l-2] << 8 | inPtr[l-3] << 16 | inPtr[l-4] << 24;
}

static bool is_fixed_bytes_type(ABI_t t) {
  return t.type >= ABI_BYTES1 && t.type <= ABI_BYTES32;
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
static bool is_elementary_type_array(ABI_t t) {
  return ((true == is_valid_abi_type(t) &&
          (false == is_dynamic_abi_type(t)) &&
          (true == t.isArray)));
}

static bool is_dynamic_type_array(ABI_t t) {
  return ((true == is_valid_abi_type(t)) &&
          (true == is_dynamic_abi_type(t)) &&
          (true == t.isArray));
}

// Get the number of bytes describing an elementary data type
static size_t elem_sz(ABI_t t) {
  if (true == is_dynamic_abi_type(t))
    return 0;
  if (true == is_fixed_bytes_type(t))
    return 1 + (t.type - ABI_BYTES1);
  switch (t.type) {
    // Non-numerical
    case ABI_ADDRESS:
      return 20;
    case ABI_BOOL:
      return 1;
    case ABI_FUNCTION:
      return 24;
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
static size_t decode_elem_param(void * out, size_t outSz, ABI_t type, void * in, size_t off) {
  // Ensure there is space for this data in `out` and that this is an elementary type
  if ((ABI_WORD_SZ > outSz) || (false == is_valid_abi_type(type)) || (true == is_dynamic_abi_type(type)))
    return 0;
  size_t nBytes = elem_sz(type);
  uint8_t * inPtr = in;
  if (outSz < nBytes)
    return 0;
  // Non-numerical (and non-bool) types have data written to the beginning of the word
  if (true == is_fixed_bytes_type(type) || type.type == ABI_ADDRESS || type.type == ABI_FUNCTION)
    memcpy(out, inPtr + off, nBytes);
  // Numerical types have data written at the end of the word 
  else
    memcpy(out, inPtr + off + (ABI_WORD_SZ - nBytes), nBytes);
  return nBytes;
}

// Decode a "non-elementary" param. This includes dynamic types as well as both fixed and vairable
// sized arrays containing a set of params of a single elementary type.
static size_t decode_non_elem_param(void * out, size_t outSz, ABI_t type, void * in, size_t off, ABISelector_t info, size_t d) {
  if (false == is_valid_abi_type(type))
    return 0;
  uint8_t * inPtr = in;
  size_t numElem = 0;
  if (is_elementary_type_array(type)) {
    // Handle elementary type arrays. Each element in these arrays is of size ABI_WORD_SZ.
    // For fixed size arrays, the number of elements is defined in the ABI itself.
    // For variable size arrays, the number of elements is encoded in the offset word.
    numElem = type.arraySz;
    if (numElem == 0) {
      numElem = get_abi_u32_be(in, off);
      off += ABI_WORD_SZ; // Account for this word, which tells us the number of ensuing elements.
    }
    // Copy the data...
    if (type.extraDepth > d) {
      // If we need to go to a higher dimension, get the offset of the nested array we want to look up
      // and bump the dimension before recursing.
      if (info.subIdx[d] >= numElem)
        return 0;
      // Skip past the elements in this nested array.
      // The first word captures the number of ensuing elements and then we skip that
      // number of additional words which contain the data in this nested array.
      for (size_t i = 0; i < info.subIdx[d]; i++)
        off += (ABI_WORD_SZ * (1 + get_abi_u32_be(in, off)));
      // Recurse with the updated dimension and new offset, which now points to the
      // beginning of the next-dimension nested array.
      return decode_non_elem_param(out, outSz, type, in, off, info, d+1);
    } 
    // If we are on the highest dimension, grab the element corresponding to that index
    if (info.subIdx[d] >= numElem)
      return 0;
    return decode_elem_param(out, outSz, type, in, off + (ABI_WORD_SZ * info.subIdx[d]));
  }
  // For dynamic types, the param offset word contains the number of *bytes*, and we can copy them directly.
  // Dynamic arrays have an additional word in front of each element, which describes
  // the number of bytes in that element. Note that these elements are written in 32
  // byte words, but may take multiple words (e.g. a 36 byte array would take 2 words).
  if (true == is_dynamic_type_array(type)) {


    // TODO: ACCOUNT FOR FIXED SIZE DYNAMIC ARRAYS >1D

    // Get the number of elements in the array of this dimension and bump the offset
    numElem = get_abi_u32_be(in, off);
    off += ABI_WORD_SZ;
    if (info.subIdx[d] >= numElem)
      return 0;

    // If we need to recurse to a higher dimension, skip to the offset that starts
    // the nested array we want at the next dimension.
    if (type.extraDepth > d) {
      // Skip to the i-th array in *this* dimension.
      for (size_t i = 0; i < info.subIdx[d]; i++) {
        size_t _numElem = get_abi_u32_be(in, off);
        off += ABI_WORD_SZ;
        // Skip all elements in this nested array
        for (size_t j = 0; j < _numElem; j++) {
          size_t arraySz = get_abi_u32_be(in, off);
          size_t numWords = 1 + (arraySz / ABI_WORD_SZ);
          // Skip the size word and all words containing this element
          off += (1 + numWords) * ABI_WORD_SZ; 
        }
      }
      // Now we can recurse.
      return decode_non_elem_param(out, outSz, type, in, off, info, d+1);
    } 
    // If we are on the highest dimension, skip past the variable sized elements
    // that come before the one we want to fetch.
    for (size_t i = 0; i < info.subIdx[d]; i++) {
      size_t arraySz = get_abi_u32_be(in, off);
      size_t numWords = 1 + (arraySz / ABI_WORD_SZ);
      // Skip the size word and all words containing this element
      off += (1 + numWords) * ABI_WORD_SZ; 
    }
  }
  // We should now be at the offset corresponding to the size of the dynamic
  // type element that we want.
  size_t elemSz = get_abi_u32_be(in, off);
  off += ABI_WORD_SZ;
  if (outSz < elemSz)
    return 0;
  memcpy(out, inPtr + off, elemSz);
  return elemSz;
}

//===============================================
// API
//===============================================

bool is_valid_abi_type(ABI_t t) {
  return t.type < ABI_MAX && t.type > ABI_NONE;
}

size_t abi_decode_param(void * out, size_t outSz, ABI_t * types, ABISelector_t info, void * in) {
  if (out == NULL || types == NULL || in == NULL)
    return 0;

  // Ensure we have valid types passed
  ABI_t type = types[info.typeIdx];
  if (false == is_valid_abi_type(type))
    return 0;
  
  // Offset of the word in the encoded blob. Per ABI spec, the first `n` words correspond
  // to the function params. These words contain parameter data if the parameter in question
  // is of elementary type. If it is a dynamic type param, the word contains information about
  // the offset containing the raw data.
  size_t wordOff = ABI_WORD_SZ * info.typeIdx;

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
  return decode_non_elem_param(out, outSz, type, in, off, info, 0);
}