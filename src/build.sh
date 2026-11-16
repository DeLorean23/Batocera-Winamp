#!/bin/bash
# Compiles the DeLorean23 Winamp plugin and the start helper (needs mingw-w64, 32-bit)
cd "$(dirname "$0")"
i686-w64-mingw32-gcc -O2 -s -shared -static-libgcc -o gen_delorean.dll gen_delorean.c -lwinmm -lgdi32 -luser32
i686-w64-mingw32-gcc -O2 -s -municode -mwindows -static-libgcc -o delorean_start.exe delorean_start.c -ladvapi32
