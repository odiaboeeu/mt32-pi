#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
SC55HOME="$ROOT/external/Nuked-SC55"

SC55_TARGET="${SC55_TARGET:-pi3}"

case "$SC55_TARGET" in
    pi3)
        SC55_CPU_FLAGS="-mcpu=cortex-a53+crc+simd"
        ;;
    pi4)
        SC55_CPU_FLAGS="-mcpu=cortex-a72+crc+simd"
        ;;
    pi5)
        SC55_CPU_FLAGS="-mcpu=cortex-a76+crc+simd"
        ;;
    *)
        echo "Unsupported SC55_TARGET: $SC55_TARGET" >&2
        echo "Valid targets: pi3, pi4, pi5" >&2
        exit 1
        ;;
esac

SC55BUILDDIR="${SC55BUILDDIR:-$ROOT/build-sc55-$SC55_TARGET}"

CXX="${CXX:-aarch64-none-elf-g++}"
AR="${AR:-aarch64-none-elf-ar}"

echo "SC55 target: $SC55_TARGET"
echo "SC55 CPU flags: $SC55_CPU_FLAGS"
echo "SC55 build directory: $SC55BUILDDIR"

CXXFLAGS="-DNUKED_SC55_HEADLESS \
-DNUKED_SC55_HEADLESS_MK2_ONLY \
-DNUKED_SC55_HEADLESS_SKIP_ANALOG \
-DNUKED_SC55_HEADLESS_CYCLES_PER_STEP=24 \
-std=c++17 \
-Ofast \
-DNDEBUG \
-fomit-frame-pointer \
-fno-unwind-tables \
-fno-asynchronous-unwind-tables \
$SC55_CPU_FLAGS \
-ffreestanding \
-fno-exceptions \
-fno-rtti \
-fno-threadsafe-statics \
-ffunction-sections \
-fdata-sections \
-I$SC55HOME/src"

mkdir -p "$SC55BUILDDIR"

SOURCES="
src/mcu_interrupt.cpp
src/mcu_opcodes.cpp
src/mcu_timer.cpp
src/pcm.cpp
src/submcu.cpp
src/mcu.cpp
src/sc55_headless_stubs.cpp
"

OBJECTS=""

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
