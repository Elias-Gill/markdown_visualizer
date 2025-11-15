# Markdown Visualizer

A small markdown visualizer written in C.
Renders markdown files using **Raylib**, **Clay**, and **MD4C**.

## Requirements

You need a C toolchain such as GCC or Clang, plus CMake and Make.
FreeType is also required and must be available on your system.

## Building

Generate the build directory:

```
cmake -B build [-DWHITE_MODE=ON|OFF]
```

Then compile:

```
cd build
make
```

After building, the executable `markdown_visualizer` will be in the same
directory along with the copied `resources` folder.

## Notes

This project has been tested just for Linux. Building on Windows or macOS has not been validated.
