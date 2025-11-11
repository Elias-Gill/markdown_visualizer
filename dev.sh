#!/usr/bin/env bash
set -e

BUILD_TYPE="Release"
WHITE_MODE="OFF"  # Por defecto modo oscuro

for arg in "$@"; do
    case "$arg" in
        --debug)
            BUILD_TYPE="Debug"
            ;;
        --dark)
            WHITE_MODE="OFF"
            ;;
        --light)
            WHITE_MODE="ON"
            ;;
    esac
done

for cmd in "$@"; do
    case "$cmd" in
        format)
            astyle -n --max-code-length=90 src/*.c src/*.h
            ;;
        build)
            cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DWHITE_MODE=$WHITE_MODE
            make --directory=./build
            ;;
        run)
            ./build/markdown_visualizer test.md
            ;;
        --debug|--dark|--light)
            ;; # ya manejados arriba, ignorar aquí
        *)
            echo "Unknown command: $cmd"
            ;;
    esac
done
