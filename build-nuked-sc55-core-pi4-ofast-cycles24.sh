#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
SC55HOME="$ROOT/external/Nuked-SC55"
SC55BUILDDIR="$ROOT/build-sc55"

CXX="${CXX:-aarch64-none-elf-g++}"
AR="${AR:-aarch64-none-elf-ar}"

CXXFLAGS="-DNUKED_SC55_HEADLESS \
-DNUKED_SC55_HEADLESS_MK2_ONLY \
-DNUKED_SC55_HEADLESS_CYCLES_PER_STEP=24 \
-std=c++17 \
-Ofast \
-DNDEBUG \
-fomit-frame-pointer \
-fno-unwind-tables \
-fno-asynchronous-unwind-tables \
-mcpu=cortex-a72+crc+simd \
-ffreestanding \
-fno-exceptions \
-fno-rtti \
-fno-threadsafe-statics \
-ffunction-sections \
-fdata-sections \
-I$SC55HOME/src"

SOURCES="
src/mcu_interrupt.cpp
src/mcu_opcodes.cpp
src/mcu_timer.cpp
src/pcm.cpp
src/submcu.cpp
src/mcu.cpp
src/sc55_headless_stubs.cpp
"

rm -rf "$SC55BUILDDIR"
mkdir -p "$SC55BUILDDIR"

OBJECTS=""

echo "SC55 source commit: $(git -C "$SC55HOME" rev-parse --short HEAD)"
echo "SC55 CPU: Cortex-A72"
echo "SC55 flags: $CXXFLAGS"

for f in $SOURCES; do
    base="$(basename "$f").o"
    obj="$SC55BUILDDIR/$base"

    echo "SC55 CXX $f"
    "$CXX" $CXXFLAGS -c "$SC55HOME/$f" -o "$obj"

    OBJECTS="$OBJECTS $obj"
done

echo "SC55 AR libnukedsc55_core.a"
"$AR" rcs "$SC55BUILDDIR/libnukedsc55_core.a" $OBJECTS

echo
ls -lh "$SC55BUILDDIR/libnukedsc55_core.a"
