# Contributing

Run `npm ci` from the SDK checkout to enable the Husky pre-commit hook. Use
Node.js 22.22.1 or later and install CMake, a C/C++ compiler, and OpenSSL
development headers for the host tests.

The hook checks staged whitespace, formats supported staged files with Prettier,
and runs the CMake host tests. Run `npm test` to run the host tests separately.
