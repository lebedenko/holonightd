#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
lane=$1
mkdir /work/source
cp -a /input/. /work/source/
cd /work/source
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
export JOBS=${JOBS:-4} CMAKE_BUILD_PARALLEL_LEVEL=${JOBS:-4}
mkdir -p "$HOME"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
mkdir -p "build/provenance/$lane"
pacman -Q > "build/provenance/$lane/packages.txt"
{ c++ --version; cmake --version; ninja --version; python3 --version; task --version; clang-format --version; clang-tidy --version; gcovr --version; } > "build/provenance/$lane/tools.txt" 2>&1
capture() {
  status=$?
  cp -a build/provenance /output/
  for preset in test release coverage; do
    if [ -d "build/$preset" ]; then
      mkdir -p "/output/$lane/$preset"
      for file in CMakeCache.txt compile_commands.json dependency-state.json external-state.json; do
        if [ -f "build/$preset/$file" ]; then cp "build/$preset/$file" "/output/$lane/$preset/"; fi
      done
      if [ -d "build/$preset/Testing" ]; then cp -a "build/$preset/Testing" "/output/$lane/$preset/"; fi
      if [ -d "build/$preset/coverage" ]; then cp -a "build/$preset/coverage" "/output/$lane/$preset/"; fi
    fi
  done
  exit "$status"
}
trap capture 0
preset=test
case "$lane" in
  systemd-off) export HOLONIGHT_BUILD_FEATURES=BUILD_SYSTEMD=OFF ;;
  release) preset=release ;;
  coverage) preset=coverage ;;
  build-test|static-checks) ;;
  *) echo "Unknown lane: $lane" >&2; exit 2 ;;
esac
task deps PRESET="$preset"
case "$lane" in
  build-test|systemd-off)
    if [ "$lane" = build-test ]; then
      python3 scripts/ci/test_launcher.py
      python3 scripts/ci/test_dependencies.py
    fi
    task test PRESET=test
    cat build/test/Testing/Temporary/LastTest.log
    ;;
  static-checks)
    task build PRESET=test
    task format-check
    task tidy-src
    task tidy-tests
    ;;
  coverage)
    task coverage
    test -s build/coverage/coverage/index.html
    ;;
  release)
    task build PRESET=release
    stage=/work/install
    DESTDIR="$stage" cmake --install build/release
    python3 scripts/ci/check_install.py "$stage/usr"
    ;;
esac
