#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FREERTOS_DIR="${ROOT_DIR}/FreeRTOS"
FREERTOS_REPO="https://github.com/FreeRTOS/FreeRTOS.git"
FORCE=0

usage() {
    cat <<USAGE
Usage: ./setup.sh [--force]

Prepare local build dependencies for this project.

This script only downloads/updates the external FreeRTOS dependency. It does not
rewrite files under src/, cmake/, or the project root. If a non-empty local
FreeRTOS/ directory is present but is not a git checkout, the script asks before
removing it; pass --force to overwrite without prompting.
USAGE
}

for arg in "$@"; do
    case "${arg}" in
        --force)
            FORCE=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: ${arg}" >&2
            usage >&2
            exit 2
            ;;
    esac
done

require_tool() {
    local tool="$1"
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "ERROR: '${tool}' is required but was not found in PATH." >&2
        echo "Install the prerequisites, for example:" >&2
        echo "  sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi gdb-multiarch qemu-system-arm cmake make git" >&2
        exit 1
    fi
}

warn_if_missing() {
    local tool="$1"
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "WARNING: '${tool}' was not found in PATH; dependency download can continue, but build/run steps will need it." >&2
    fi
}

confirm_overwrite() {
    local path="$1"

    if [ "${FORCE}" -eq 1 ]; then
        return 0
    fi

    if [ ! -t 0 ]; then
        echo "ERROR: ${path} already exists and is not an empty git checkout." >&2
        echo "Refusing to overwrite it in a non-interactive shell. Re-run with --force if this is intentional." >&2
        exit 1
    fi

    read -r -p "${path} already exists and is not managed by git. Remove and re-clone it? [y/N] " answer
    case "${answer}" in
        y|Y|yes|YES)
            return 0
            ;;
        *)
            echo "Aborted; left ${path} untouched." >&2
            exit 1
            ;;
    esac
}

echo "=== Checking host tools ==="
require_tool git
warn_if_missing cmake
warn_if_missing make
warn_if_missing arm-none-eabi-gcc
warn_if_missing qemu-system-arm

echo "=== Preparing FreeRTOS dependency ==="
cd "${ROOT_DIR}"

if [ ! -e "${FREERTOS_DIR}" ]; then
    echo "FreeRTOS/ not found; cloning ${FREERTOS_REPO} ..."
    git clone --depth 1 --recurse-submodules --shallow-submodules "${FREERTOS_REPO}" "${FREERTOS_DIR}"
elif [ -d "${FREERTOS_DIR}/.git" ]; then
    echo "FreeRTOS/ is an existing git checkout; updating with fast-forward only ..."
    git -C "${FREERTOS_DIR}" pull --ff-only
    git -C "${FREERTOS_DIR}" submodule update --init --recursive --depth 1 FreeRTOS/Source
elif [ -d "${FREERTOS_DIR}" ] && [ -z "$(find "${FREERTOS_DIR}" -mindepth 1 -maxdepth 1 -print -quit)" ]; then
    echo "FreeRTOS/ exists but is empty; replacing it with a fresh clone ..."
    rmdir "${FREERTOS_DIR}"
    git clone --depth 1 --recurse-submodules --shallow-submodules "${FREERTOS_REPO}" "${FREERTOS_DIR}"
else
    confirm_overwrite "${FREERTOS_DIR}"
    rm -rf "${FREERTOS_DIR}"
    git clone --depth 1 --recurse-submodules --shallow-submodules "${FREERTOS_REPO}" "${FREERTOS_DIR}"
fi

git -C "${FREERTOS_DIR}" submodule update --init --recursive --depth 1 FreeRTOS/Source

if [ ! -f "${FREERTOS_DIR}/FreeRTOS/Source/tasks.c" ]; then
    echo "ERROR: FreeRTOS kernel source was not found at:" >&2
    echo "  ${FREERTOS_DIR}/FreeRTOS/Source/tasks.c" >&2
    echo "Try removing FreeRTOS/ and running ./setup.sh again." >&2
    exit 1
fi

cat <<DONE

Setup complete.

Next steps:
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake
  cmake --build build
DONE
