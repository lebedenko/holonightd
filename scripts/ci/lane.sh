#!/bin/sh
set -eu
lane=$1
mkdir /work/source
cp -a /input/. /work/source/
cd /work/source
export HOME=/work/build/home LC_ALL=C TZ=UTC
mkdir -p "$HOME"
case "$lane" in
  licensing)
    reuse --version
    reuse lint
    ;;
  build-test|static-checks)
    python3 --version
    git --version
    cmake --version
    ninja --version
    c++ --version
    clang-format --version
    clang-tidy --version
    cmake -S . -B build/verification -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTS=ON
    if [ "$lane" = build-test ]; then
      python3 scripts/ci/test_launcher.py
      cmake --build build/verification --parallel
      ctest --test-dir build/verification --output-on-failure --no-tests=error
    else
      cmake --build build/verification --target format-check
      cmake --build build/verification --target tidy
    fi
    ;;
  *) echo "Unknown lane: $lane" >&2; exit 2 ;;
esac
