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

#define ABI_WORD_SZ 32
#define ABI_ARRAY_DEPTH_MAX 5

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
  ABIAtomic_t type;                     // The underlying, atomic type
  bool isArray;                         // Whether this is an array of the atomic type
  size_t arraySz;                       // Non-zero implies fixed size array and describes the size. 
                                        // (Only 1D fixed size arrays allowed)
  size_t extraDepth;                    // Number of extra dimensions in the array, if applicable 
                                        // (e.g. 2D array -> extraDepth=1)
} ABI_t;

typedef struct {
  size_t typeIdx;                     // The index of the type param in the function definition
  size_t subIdx[ABI_ARRAY_DEPTH_MAX]; // The set of indices describing the multi-dimensional array element
                                      // to fetch, e.g. array[0][1][6] would be [ 0, 1, 6, 0, 0 ]
                                      // Note that this util will not fetch any indices beyond `1+extraDepth`
                                      // as described in the ABI_t type, so if you had a 2D array and passed
                                      // indices [0,1,2,3,4], it would only lookup array[0][1].
                                      // Similarly, a 3D array with indices [1, 0, 0, 0, 0] would fetch array[1][0][0],
                                      // meaning data can only be fetched from the highest dimension of the array.
                                      // NOTE: For fixed-size arrays (ABI_t->arraySz = 0), all indices beyond
                                      // subIdx[0] are ignored.
} ABISelector_t;

// Ensure we have a valid ABI schema being passed. We check the following:
// * Is each atomic type a valid ABI type? (e.g. uint32, string)
// * Is each type an single element or array (fixed or dynamic)?
// Note that for arrays, we only support all fixed or all dynamic dimensions,
// meaning things like `string[3][3]` and `string[]` are allowed, but
// `string[3][]` are not. This is because the ABI spec is pretty loose about
// defining these encodings, so we will just be strict and reject combinations.
// @param `tyes`      - array of types making up the schema
// @param `numTypes`  - number of types in the schema
// @return            - true if we can handle every type in this schema
bool is_valid_abi_schema(ABI_t * types, size_t numTypes);

// Decode and return a param's data in `out` given a set of ABI types and an `in` buffer.
// Note that padding is stripped from elementary types, which are encoded in 32-byte words regardless
// of the underlying data size. For example, a single ABI_BOOL would be the last byte of a
// 32 byte word. Dynamic types are returned in full, as there is no padding.
// @param `out`       - output buffer to be written
// @param `outSz`     - size of output buffer to be written
// @param `types`     - array of ABI type definitions
// @param `numTypes`  - the number of types in this ABI definition
// @param `info`      - information about the data to be selected
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - number of bytes written to `out`; 0 on error.
size_t abi_decode_param(void * out, 
                        size_t outSz, 
                        ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        void * in,
                        size_t inSz);

#endif