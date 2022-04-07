#!/usr/bin/env bash

set -eu

flags=(
    "-DDEBUG"
    "-ferror-limit=1"
    "-fno-autolink"
    "-fno-exceptions"
    "-fno-math-errno"
    "-fno-omit-frame-pointer"
    "-fno-rtti"
    "-fno-unwind-tables"
    "-fsanitize=address"
    "-fsanitize=undefined"
    "-fshort-enums"
    "-fuse-ld=lld"
    "-g"
    "-march=native"
    "-O1"
    "-std=c++11"
    "-Werror"
    "-Weverything"
    "-Wno-c++98-compat-pedantic"
    "-Wno-c99-extensions"
    "-Wno-covered-switch-default"
    "-Wno-extra-semi-stmt"
    "-Wno-padded"
    "-Wno-reserved-id-macro"
)

now () {
    date +%s.%N
}

(
    start=$(now)
    clang-format -i -verbose "$WD/src"/*
    mold -run clang++ "${flags[@]}" -o "$WD/bin/main" "$WD/src/main.cpp"
    end=$(now)
    python3 -c "
from sys import stderr
print(\"Compiled! ({:.3f}s)\n\".format($end - $start), file=stderr)
    "
)

time "$WD/bin/main" "$WD/examples/Sum.asm" \
    "$WD/examples/Sum.hack"
time "$WD/bin/main" "$WD/nand2tetris/projects/06/add/Add.asm" \
    "$WD/nand2tetris/projects/06/add/Add.hack"
time "$WD/bin/main" "$WD/nand2tetris/projects/06/max/Max.asm" \
    "$WD/nand2tetris/projects/06/max/Max.hack"
time "$WD/bin/main" "$WD/nand2tetris/projects/06/rect/Rect.asm" \
    "$WD/nand2tetris/projects/06/rect/Rect.hack"
time "$WD/bin/main" "$WD/nand2tetris/projects/06/pong/Pong.asm" \
    "$WD/nand2tetris/projects/06/pong/Pong.hack"
