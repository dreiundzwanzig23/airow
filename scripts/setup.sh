#!/usr/bin/env bash
# setup.sh — Ubuntu-only, Clang-first. Supports libc++ (default) or libstdc++.
# Usage:
#   ./scripts/setup.sh                 # clang + libc++
#   ./scripts/setup.sh --stdlib=libstdc++   # clang + libstdc++
#   ./scripts/setup.sh --no-build      # install only
set -euo pipefail

NO_BUILD=0
STDLIB="libc++"

for arg in "$@"; do
  case "$arg" in
    --no-build) NO_BUILD=1 ;;
    --stdlib=libc++|--stdlib=libstdc++) STDLIB="${arg#--stdlib=}" ;;
    *) echo "Unknown arg: $arg" ; exit 2 ;;
  esac
done

echo "[INFO] Installing Ubuntu build dependencies (apt) …"
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git llvm clang clang-tidy lld gdb lldb pkg-config libgtest-dev nlohmann-json3-dev python3-pip

echo "[INFO] Installing Python-based quality tools …"
python3 -m pip install --user lizard --upgrade

if [[ "$STDLIB" == "libc++" ]]; then
  sudo apt-get install -y libc++-dev libc++abi-dev
  CMAKE_ARGS=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++")
  echo "[INFO] Using clang + libc++"
else
  # libstdc++: ensure the dev toolchain that matches the system-default GCC
  DEFAULT_MAJOR="$(gcc -dumpversion | cut -d. -f1 || echo 13)"
  sudo apt-get install -y "g++-${DEFAULT_MAJOR}" "libstdc++-${DEFAULT_MAJOR}-dev" || sudo apt-get install -y g++ libstdc++-13-dev
  # Compute the default GCC libdir dynamically (no explicit versions)
  DEFAULT_LIB="$(gcc -print-file-name=libstdc++.so)"
  DEFAULT_LIBDIR="$(dirname "$DEFAULT_LIB")"
  echo "[INFO] Using clang + libstdc++ (system default GCC=$DEFAULT_MAJOR, libdir=$DEFAULT_LIBDIR)"
  TRIPLE="$(gcc -dumpmachine)"
  INC1="/usr/include/c++/${DEFAULT_MAJOR}"
  INC2="/usr/include/${TRIPLE}/c++/${DEFAULT_MAJOR}"
  echo "[INFO] Using clang + libstdc++ includes: $INC1, $INC2"
  CMAKE_ARGS=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_EXE_LINKER_FLAGS="-L${DEFAULT_LIBDIR}" -DCMAKE_CXX_FLAGS="-isystem ${INC1} -isystem ${INC2}")
fi

echo "[INFO] Configuration summary:"
echo "       stdlib=${STDLIB}"
if [[ "${NO_BUILD}" -eq 1 ]]; then
  echo "       build_and_test=no"
else
  echo "       build_and_test=yes"
fi

echo "[INFO] Configuring (Debug, Ninja) …"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug "${CMAKE_ARGS[@]}"

if [[ "$NO_BUILD" -eq 0 ]]; then
  echo "[INFO] Building …"
  cmake --build build -j
  echo "[INFO] Running tests …"
  ctest --test-dir build --output-on-failure
else
  echo "[INFO] Skipping build (--no-build)."
fi
