# HelloGitHub

A minimal C++ "Hello, GitHub!" program, originally set up as an Eclipse CDT project.

## Requirements

- A C++ compiler (g++ / clang++)
- Either CMake (>= 3.10) **or** GNU Make

## Build & run

### Using CMake

```sh
cmake -S . -B build
cmake --build build
./build/hello
```

### Using Make

```sh
make
./hello
```

### Directly with g++

```sh
g++ hello.cpp -o hello
./hello
```

Expected output:

```
Hello, GitHub!
```

## Development

Source formatting is enforced with `clang-format` via [pre-commit](https://pre-commit.com/):

```sh
pip install pre-commit
pre-commit install
pre-commit run --all-files
```

## Continuous integration

Every push and pull request is built on Ubuntu via GitHub Actions — see [`.github/workflows/ci.yml`](.github/workflows/ci.yml).
