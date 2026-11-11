#!/bin/bash
# Builds and runs the eez-framework tests (Linux / WSL).
#
#   tests/run.sh [--no-sanitize | --tsan] [--rebuild] [runner options] [filter...]
#
# --tsan builds with ThreadSanitizer instead of AddressSanitizer and
# UndefinedBehaviorSanitizer (for the data race tests, e.g. "tsan").
# Runner options: --list, --verbose, --no-fork, --timeout=S (harness/test.cpp).
# The build goes to ~/.cache/eez-framework-tests (EEZ_TESTS_BUILD_DIR), one folder
# per tests folder and build variant. EEZ_FW_SRC selects the framework src folder
# to test (default: ../src next to this folder).

set -e

HERE="$(cd "$(dirname "$0")" && pwd)"

SANITIZE=ON
TSAN=OFF
REBUILD=
ARGS=()
for arg in "$@"; do
    case "$arg" in
        --no-sanitize) SANITIZE=OFF ;;
        --tsan) SANITIZE=OFF; TSAN=ON ;;
        --rebuild) REBUILD=1 ;;
        *) ARGS+=("$arg") ;;
    esac
done

VARIANT=$([ "$TSAN" = ON ] && echo tsan || ([ "$SANITIZE" = ON ] && echo asan || echo plain))
BUILD="${EEZ_TESTS_BUILD_DIR:-$HOME/.cache/eez-framework-tests}/$(echo "$HERE" | md5sum | cut -c1-8)-$VARIANT"

if [ -n "$REBUILD" ]; then
    rm -rf "$BUILD"
fi

FW_SRC="${EEZ_FW_SRC:-$HERE/../src}"

cmake -S "$HERE" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DEEZ_TESTS_SANITIZE=$SANITIZE -DEEZ_TESTS_TSAN=$TSAN -DFW_SRC="$FW_SRC" > /dev/null
cmake --build "$BUILD" -j"$(nproc)"

# leaks are not checked yet (a test process ends with _exit)
export ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
export UBSAN_OPTIONS="print_stacktrace=1${UBSAN_OPTIONS:+:$UBSAN_OPTIONS}"

if [ "$TSAN" = ON ]; then
    # ThreadSanitizer fails with "unexpected memory mapping" with the high
    # mmap randomization of newer kernels (WSL): run without ASLR
    exec setarch "$(uname -m)" -R "$BUILD/eez-framework-tests" "${ARGS[@]}"
fi

"$BUILD/eez-framework-tests" "${ARGS[@]}"
