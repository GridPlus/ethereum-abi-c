# ethereum-abi-c

A util for interacting with [Ethereum ABI](https://docs.soliditylang.org/en/develop/abi-spec.html) data.

This repo contains only a limited number of tools specific to what is needed by GridPlus, but it may be useful for someone else
as we could not find any C tooling for handling ABI data in Ethereum.

## API

The following functionality is exposed via the `abi.h` API:

```
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
```

## Testing

We provide a number of tests based on the [examples in the ABI spec](https://docs.soliditylang.org/en/develop/abi-spec.html#examples).
You can run the tests with:

```
make test && ./test
```