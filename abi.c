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
// but may contain less data than 32 bytes (depending on the type -- see `elemSz()`).
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

// Decode a parameter of dynamic type. Each dynamic type is prefixed with a word that
// specifies the size of the item, followed by `N` words worth of data. If the param
// is not a multiple of 32 bytes, it is right-padded with zeros.
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

// Decode a param given its offset. The rules for decoding depend on the type of param.
// The offset provided (`off`) is the starting place of the param itself. 
static size_t decode_param( void * out, 
                            size_t outSz, 
                            ABI_t type, 
                            const void * in, 
                            size_t inSz, 
                            size_t off, 
                            ABISelector_t info,
                            bool szOnly) 
{
  while(out == NULL || in == NULL);
  // Elementary types are fairly straight forward
  if (is_elementary_type_variable_sz_array(type)) {
    // Variable sized arrays require a jump to the item
    size_t numElem = get_abi_u32_be(in, off);
    if (info.arrIdx >= numElem)
      return 0;
    // Skip the numElem word and jump to the array index
    off += ABI_WORD_SZ * (1 + info.arrIdx);
    return decode_elem_param(out, outSz, type, in, inSz, off);
  } else if (is_elementary_atomic_type(type)) {
    // Other elementary types can be decoded without modification
    return decode_elem_param(out, outSz, type, in, inSz, off);
  }

  // Dynamic types have prefixes that we need to account for
  if (true == is_dynamic_type_array(type)) {
    if (true == is_dynamic_type_fixed_sz_array(type)) {
      off += get_abi_u32_be(in, off + (ABI_WORD_SZ * info.arrIdx));
    } else {
      // Sanity check to avoid overrun
      if (off + ABI_WORD_SZ > inSz)
        return 0;
      // Another sanity check to avoid overrun
      size_t numElem = get_abi_u32_be(in, off);
      if (info.arrIdx >= numElem)
        return 0;
      // Skip past this word
      off += ABI_WORD_SZ;
      // Get the offset for this item and jump to it
      off += get_abi_u32_be(in, off + (ABI_WORD_SZ * info.arrIdx));
    }
  }
  // We should now be at the offset corresponding to the size of the dynamic
  // type element that we want.
  return decode_dynamic_param(out, outSz, type, in, inSz, off, szOnly);
}

// Get an offset of the parameter in question. The rules depend on the type of param.
static size_t get_param_offset(const ABI_t * types, size_t numTypes, ABISelector_t info, const void * in, size_t inSz) {
  while (types == NULL || in == NULL);
  ABI_t type = types[info.typeIdx];
  size_t off = 0;
  size_t paramOff = 0;
  // Fixed size elementary type arrays pack everything into the header data, i.e.
  // do not come with an offset to the param. If such a param comes before our target
  // param we need to sufficiently skip over it.
  for (size_t i = 0; i < info.typeIdx; i++) {
      // Note the -1, which accounts for the param in this slot. If this was a normal
      // param it would take up 1 word.
    if (true == is_elementary_type_fixed_sz_array(types[i]))
      off += ABI_WORD_SZ * (types[i].arraySz - 1);
  }
  // Dynamic types and variable sized arrays of any type are located at 
  // their respective offsets
  if ((true == is_dynamic_atomic_type(type)) || 
      (true == is_variable_sz_array(type)))
    paramOff = get_abi_u32_be(in, off + (ABI_WORD_SZ * info.typeIdx));
  // All other parameters are located in the header
  else
    paramOff = off + (ABI_WORD_SZ * info.typeIdx);

  // Fixed size elementary type arrays are a special case. The offset we have corresponds
  // to the first param in the array so we just need to skip words to locate the individual
  // item we want.
  if (true == is_elementary_type_fixed_sz_array(type)) {
    // Sanity check to avoid overrun
    if (type.arraySz <= info.arrIdx)
      return inSz + 1;
    return paramOff + (ABI_WORD_SZ * info.arrIdx);
  }
  // Sanity check to avoid overrun
  if (true == is_dynamic_type_fixed_sz_array(type) && type.arraySz <= info.arrIdx)
      return inSz + 1;
  // We now have the offset of the data we want
  return paramOff;
}

//===============================================
// API
//===============================================
bool abi_is_valid_schema(const ABI_t * types, size_t numTypes) {
  while(types == NULL);
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
  while(types == NULL || in == NULL);
  ABI_t type = types[info.typeIdx];
  if ((info.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)) ||
      (false == is_variable_sz_array(type)) )
    return 0;
  // Get the offset of this parameter in the function definition
  size_t paramOff = get_param_offset(types, numTypes, info, in, inSz);
  if (paramOff > inSz)
    return 0;
  // The parameter at `paramOff` contains an offset where the array
  // data begins. Since the first word in a variable sized array
  // (which this must be) contains the array size, that's what we need.
  return get_abi_u32_be(in, paramOff);
}

size_t abi_decode_param(void * out, 
                        size_t outSz, 
                        const ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        const void * in,
                        size_t inSz) 
{
  while(out == NULL || types == NULL || in == NULL);
  // Ensure we have valid types passed
  if ((info.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)))
    return 0;
  size_t paramOff = get_param_offset(types, numTypes, info, in, inSz);
  if (paramOff > inSz)
    return 0;
  return decode_param(out, outSz, types[info.typeIdx], in, inSz, paramOff, info, false);
}

size_t abi_get_param_sz(const ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        const void * in, 
                        size_t inSz)
{
  while(types == NULL || in == NULL);
  ABI_t type = types[info.typeIdx];
  if ((info.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)) ||
      (false == is_dynamic_atomic_type(type)))
    return 0;

  // We will build a fake output buffer to reuse the `decode_param` function.
  uint8_t out[1] = {0};
  uint8_t outSz = 1;
  size_t paramOff = get_param_offset(types, numTypes, info, in, inSz);
  if (paramOff > inSz)
    return 0;
  return decode_param(out, outSz, types[info.typeIdx], in, inSz, paramOff, info, true);
}

size_t abi_decode_tuple_param(void * out, 
                              size_t outSz, 
                              const ABI_t * types, 
                              size_t numTypes,
                              ABISelector_t tupleInfo,
                              ABISelector_t paramInfo, 
                              const void * in,
                              size_t inSz) 
{
  while(out == NULL || types == NULL || in == NULL);

  // Ensure we have valid types passed
  if ((tupleInfo.typeIdx >= numTypes) ||
      (false == abi_is_valid_schema(types, numTypes)))
    return 0;
  ABI_t tupleType = types[tupleInfo.typeIdx];
  // Sanity check: ensure this is a tuple type
  if (tupleType.type > ABI_TUPLE20 || tupleType.type < ABI_TUPLE1)
    return 0;

  size_t numTupleTypes = (tupleType.type - ABI_TUPLE1) + 1;
  // Sanity check: ensure we won't overrun our buffer
  if (paramInfo.typeIdx > numTupleTypes || paramInfo.typeIdx > numTypes)
    return 0;

  // Get the offset of the tuple data and shift our input buffer offset
  size_t paramOff = get_param_offset(types, numTypes, tupleInfo, in, inSz);
  if (paramOff > inSz)
    return 0;
  size_t dataOff = get_abi_u32_be(in, paramOff);
  in += dataOff;
  inSz -= dataOff;
  // Also update types pointer offset to skip non-tuple types
  const ABI_t * tupleTypes = types + (numTypes - numTupleTypes);
  // TODO: Handle tuple arrays
  return abi_decode_param(out, 
                          outSz, 
                          tupleTypes, 
                          numTupleTypes,
                          paramInfo,
                          in,
                          inSz);
}