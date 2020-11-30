/**
 * Ethereum ABI encoder/decoder
 * https://github.com/GridPlus/ethereum-abi-c
 * 
 * This library implements the Ethereum ABI spec 
 * (https://docs.soliditylang.org/en/develop/abi-spec.html)
 * as it pertains to contract method calls. We support encoding and decoding
 * of Ethereum data types.
 *
 * MIT License
 *
 * Copyright (c) 2020 Aurash Kamalipour <afkamalipour@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ETHEREUM_ABI_H_
#define __ETHEREUM_ABI_H_

#include <stdbool.h>
#include <stdlib.h>

// Enumeration of ABI types.
// * ABI_BYTES and ABI_STRING are both dynamic types and can be any size
// * Everything else is an elementary type and must be packed into a 32 byte word
typedef enum {
  ABI_NONE = 0,

  // Supported fixed types
  ABI_ADDRESS,
  ABI_BOOL,
  ABI_FUNCTION, // 20 byte address + 4 byte function selector
  ABI_UINT8,
  ABI_UINT16,
  ABI_UINT24,
  ABI_UINT32,
  ABI_UINT64,
  ABI_UINT128,
  ABI_UINT256,
  ABI_INT8,
  ABI_INT16,
  ABI_INT24,
  ABI_INT32,
  ABI_INT64,
  ABI_INT128,
  ABI_INT256,
  ABI_UINT, // alias for UINT256
  ABI_INT,  // alian for INT256
  ABI_BYTES1,
  ABI_BYTES2,
  ABI_BYTES3,
  ABI_BYTES4,
  ABI_BYTES5,
  ABI_BYTES6,
  ABI_BYTES7,
  ABI_BYTES8,
  ABI_BYTES9,
  ABI_BYTES10,
  ABI_BYTES11,
  ABI_BYTES12,
  ABI_BYTES13,
  ABI_BYTES14,
  ABI_BYTES15,
  ABI_BYTES16,
  ABI_BYTES17,
  ABI_BYTES18,
  ABI_BYTES19,
  ABI_BYTES20,
  ABI_BYTES21,
  ABI_BYTES22,
  ABI_BYTES23,
  ABI_BYTES24,
  ABI_BYTES25,
  ABI_BYTES26,
  ABI_BYTES27,
  ABI_BYTES28,
  ABI_BYTES29,
  ABI_BYTES30,
  ABI_BYTES31,
  ABI_BYTES32,

  // Supported dynamic types
  ABI_BYTES,
  ABI_STRING,

  ABI_MAX,
} ABIAtomic_t;

// Full description of an ABI type.
// * `isArray` can only be true for an elementary type and indicates the type is in an array,
//    which means we should have a set of `N` 32-byte words, where each word contains an elementary type
// * `arraySz` is only used if `isArray==true`. If `arraySz==0`, it is a dynamic sized array
typedef struct {
  ABIAtomic_t type;   // The underlying, atomic type
  bool isArray;       // Whether this is an array of the atomic type
  size_t arraySz;     // Size of the array (non-zero implies fixed size array)
} ABI_t;

// Decode and return a param's data in `out` given a set of ABI types and an `in` buffer.
// Note that padding is stripped from elementary types, which are encoded in 32-byte words regardless
// of the underlying data size. For example, a single ABI_BOOL would be the last byte of a
// 32 byte word. Dynamic types are returned in full, as there is no padding.
// @param `out`     - output buffer to be written
// @param `outSz`   - size of output buffer to be written
// @param `types`   - array of ABI type definitions
// @param `typeIdx` - index of the ABI type definition in `types` whos data we want to extract
// @param `in`      - raw input buffer containing the entire ABI-encoded message
// @return          - number of bytes written to `out`; 0 on error.
size_t abi_decode_param(void * out, size_t outSz, ABI_t * types, size_t typeIdx, void * in);

#endif