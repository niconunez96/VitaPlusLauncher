#!/usr/bin/env bash
# Build VitaShelf Launcher with a local VitaSDK installation.
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
sdk="${VITASDK:-$HOME/.local/vitasdk}"
build_dir="$root_dir/dist/build-vita-local"
vpk_path="$root_dir/dist/vitashelflauncher.vpk"
build_type="${CMAKE_BUILD_TYPE:-}"
clean=0

usage() {
    cat <<'EOF'
Usage: scripts/build-vpk-local.sh [options]

Builds dist/vitashelflauncher.vpk with a local VitaSDK installation.

Options:
  --clean             Remove dist/build-vita-local before configuring.
  --build-type TYPE   Pass -DCMAKE_BUILD_TYPE=TYPE.
  -h, --help          Show this help.

Environment:
  VITASDK             VitaSDK root (default: ~/.local/vitasdk).
  CMAKE_BUILD_TYPE    Same as --build-type.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            clean=1
            ;;
        --build-type)
            build_type="${2:?missing value for --build-type}"
            shift
            ;;
        --build-type=*)
            build_type="${1#*=}"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option '$1'" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [[ ! -f "$sdk/share/vita.toolchain.cmake" ]]; then
    echo "error: VitaSDK toolchain not found at $sdk" >&2
    exit 1
fi

export VITASDK="$sdk"
export PATH="$VITASDK/bin:$PATH"

for command in cmake arm-vita-eabi-gcc vita-mksfoex vita-pack-vpk; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "error: required command not found: $command" >&2
        exit 1
    fi
done

if ((clean)); then
    rm -rf "$build_dir"
fi

cmake_args=(
    -S "$root_dir"
    -B "$build_dir"
    -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
)
if [[ -n "$build_type" ]]; then
    cmake_args+=("-DCMAKE_BUILD_TYPE=$build_type")
fi

mkdir -p "$root_dir/dist"
echo "==> Building with VitaSDK at $VITASDK"
cmake "${cmake_args[@]}"
cmake --build "$build_dir" -j"$(nproc)"

if [[ ! -f "$build_dir/vitashelflauncher.vpk" ]]; then
    echo "error: $build_dir/vitashelflauncher.vpk was not produced" >&2
    exit 1
fi

cp "$build_dir/vitashelflauncher.vpk" "$vpk_path"
echo
echo "VPK: $vpk_path"
ls -lh "$vpk_path"
sha256sum "$vpk_path"
