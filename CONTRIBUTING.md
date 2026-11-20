# Contributing

Install CMake, a C/C++ compiler, and OpenSSL development headers for the host
tests. To enable the optional native Git hook, run this from the SDK checkout:

```sh
git config --local core.hooksPath .githooks
```

The hook checks staged whitespace and runs the CMake host tests without changing
source files. See [test/README.md](test/README.md) for manual checks, including
PlatformIO builds and board tests.
