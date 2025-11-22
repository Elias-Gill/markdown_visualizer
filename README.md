# Markdown Visualizer

A small markdown visualizer written in C.
Renders markdown files using **Raylib**, **Clay**, and **MD4C**.

<img width="937" height="942" alt="image" src="https://github.com/user-attachments/assets/eb05f74a-9e19-4f10-b2cb-46a0e173fd35" />

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

## Keybinds

It supports basic vim motions:

- j, k, h, l (Down, Up, Left, Right)
- g and G (go-to-Top, go-to-Bottom)
- d and u (half-page-Down, half-page-Up)
