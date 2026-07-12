#!/usr/bin/env bash
#
# mayhem/build.sh — libnest2d (header-mostly C++14 2D nesting library, clipper + nlopt backends).
#
# Builds:
#   1) the sanitized fuzz harness /mayhem/fuzz_libnest (harness + src/libnest2d.cpp compiled
#      together with $SANITIZER_FLAGS so the library code is instrumented)
#   2) the standalone run-once reproducer /mayhem/fuzz_libnest-standalone
#   3) the upstream Catch2 test suite /mayhem/tests_clipper_nlopt with NORMAL flags
#      (tests/test.cpp + tools/printer_parts.cpp, against the vendored Catch2 2.13.10 single
#      header at /opt/vendor — the exact version upstream's tests/CMakeLists.txt requires)
#
# Dependencies (polyclipping, nlopt, boost headers) come from pinned apt packages baked into
# the image; Catch2 is vendored in-image — the build is fully offline / re-runnable.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

COMMON_FLAGS="-std=c++14 -I$SRC/include -I/usr/include/polyclipping \
  -DLIBNEST2D_GEOMETRIES_clipper -DLIBNEST2D_OPTIMIZER_nlopt -DLIBNEST2D_THREADING_std"
LINK_LIBS="-lpolyclipping -lnlopt -lpthread"

# 1) Sanitized fuzz harness (library sources compiled in the same invocation so the fuzzed
#    code carries the sanitizers + DWARF-3 symbols).
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE $COMMON_FLAGS \
  "$SRC/mayhem/fuzz_nest.cpp" "$SRC/src/libnest2d.cpp" \
  $LINK_LIBS -o /mayhem/fuzz_libnest

# 2) Standalone (non-fuzzer) run-once reproducer. Compile the C driver as C first so its
#    LLVMFuzzerTestOneInput reference keeps C linkage.
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o /tmp/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $COMMON_FLAGS \
  "$SRC/mayhem/fuzz_nest.cpp" "$SRC/src/libnest2d.cpp" /tmp/standalone_main.o \
  $LINK_LIBS -o /mayhem/fuzz_libnest-standalone

# 3) Upstream Catch2 test suite, NORMAL flags (no sanitizers) — test.sh only RUNS it.
$CXX -O2 $COVERAGE_FLAGS $COMMON_FLAGS -I/opt/vendor \
  "$SRC/tests/test.cpp" "$SRC/tools/printer_parts.cpp" "$SRC/src/libnest2d.cpp" \
  $LINK_LIBS -o /mayhem/tests_clipper_nlopt

echo "build.sh: OK"
