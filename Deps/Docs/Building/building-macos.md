# Building Xenon for macOS

## Dependencies

Make sure you have the most recent version of Xcode or Xcode CLT installed.

Xenon needs to be built on llvm clang **not** AppleClang.  

```bash
brew install ninja cmake llvm
```

## Building

```bash
cmake -G Ninja -B build -DCMAKE_C_COMPILER=$(brew --prefix llvm)/bin/clang -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++
cmake --build build
```