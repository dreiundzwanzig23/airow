#!/usr/bin/env bash
# Provision the supported Chrono install for standard non-libc++ builds.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SOURCE_DIR="${CHRONO_SOURCE_DIR:-${REPO_ROOT}/../chrono}"
BUILD_DIR="${CHRONO_BUILD_DIR:-${REPO_ROOT}/.external/build-chrono}"
INSTALL_PREFIX="${CHRONO_INSTALL_PREFIX:-${REPO_ROOT}/.external/chrono-install}"

for arg in "$@"; do
  case "$arg" in
    --source=*) SOURCE_DIR="${arg#--source=}" ;;
    --build-dir=*) BUILD_DIR="${arg#--build-dir=}" ;;
    --prefix=*) INSTALL_PREFIX="${arg#--prefix=}" ;;
    *) echo "Unknown arg: $arg" ; exit 2 ;;
  esac
done

if [[ -f "${INSTALL_PREFIX}/lib/cmake/Chrono/ChronoConfig.cmake" ]]; then
  echo "[INFO] Supported Chrono install already present at ${INSTALL_PREFIX}"
  exit 0
fi

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
  echo "[ERROR] Chrono source not found at ${SOURCE_DIR}" >&2
  echo "[ERROR] Set CHRONO_SOURCE_DIR or pass --source=/path/to/chrono" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}" "${INSTALL_PREFIX}"

echo "[INFO] Configuring Chrono from ${SOURCE_DIR}"
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DBUILD_DEMOS=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_BENCHMARKING=OFF \
  -DCH_ENABLE_MODULE_IRRLICHT=OFF \
  -DCH_ENABLE_MODULE_PARSERS=OFF \
  -DCH_ENABLE_MODULE_VEHICLE=OFF \
  -DCH_ENABLE_MODULE_FSI=OFF \
  -DCH_ENABLE_MODULE_SENSOR=OFF \
  -DCH_ENABLE_MODULE_POSTPROCESS=OFF \
  -DCH_ENABLE_MODULE_PYTHON=OFF \
  -DCH_ENABLE_MODULE_CASCADE=OFF \
  -DCH_ENABLE_MODULE_MODAL=OFF \
  -DCH_ENABLE_MODULE_MUMPS=OFF \
  -DCH_ENABLE_MODULE_MULTICORE=OFF \
  -DCH_ENABLE_MODULE_MATLAB=OFF \
  -DCH_ENABLE_MODULE_PARDISO_MKL=OFF \
  -DCH_ENABLE_MODULE_DEM=OFF \
  -DCH_ENABLE_MODULE_SYNCHRONO=OFF \
  -DCH_ENABLE_MODULE_FMI=OFF \
  -DCH_ENABLE_MODULE_ROS=OFF \
  -DCH_ENABLE_MODULE_PERIDYNAMICS=OFF \
  -DCH_ENABLE_MODULE_VSG=OFF \
  -DCH_ENABLE_MODULE_CSHARP=OFF

echo "[INFO] Building Chrono"
cmake --build "${BUILD_DIR}" --parallel

echo "[INFO] Installing Chrono into ${INSTALL_PREFIX}"
cmake --install "${BUILD_DIR}"
