#!/usr/bin/env bash
set -e

for cmd in "$@"; do
    case "$cmd" in
        format)
            astyle -n --max-code-length=90 src/*.c src/*.h
            ;;
        build)
            cmake -B build -DWHITE_MODE=ON
            make --directory=./build
            ;;
        run)
            ./build/markdown_visualizer test.md
            ;;
        *)
            echo "Unknown command: $cmd"
            ;;
    esac
done
