#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

image="${PSPDEV_IMAGE:-$(cat Tools/psp-image.txt)}"
jobs="${PSP_BUILD_JOBS:-4}"
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "PSP_BUILD_JOBS deve ser um inteiro positivo." >&2
    exit 1
fi

case "${1:-}" in
    "")
        command -v docker >/dev/null || {
            echo "Instale e inicie o Docker, ou use --native com PSPSDK instalado." >&2
            exit 1
        }
        exec docker run --rm --platform linux/amd64 \
            --mount "type=bind,source=$repo_dir,target=/workspace" \
            --workdir /workspace \
            --env "PSP_BUILD_JOBS=$jobs" \
            --env "PSPDEV_IMAGE=$image" \
            "$image" bash scripts/psp.sh --native
        ;;
    --native) ;;
    *)
        echo "Uso: ./scripts/psp.sh [--native]" >&2
        exit 1
        ;;
esac

for tool in git make psp-g++ psp-cmake cmake; do
    command -v "$tool" >/dev/null || {
        echo "Ferramenta ausente: $tool" >&2
        exit 1
    }
done

for plugin in DveMgr ExceptionHandler MediaEngine; do
    make -j "$jobs" -C "Source/SysPSP/PRX/$plugin"
done

make -j "$jobs" -C Tools/psp-volume

psp-cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$jobs"

# Install into a clean staging directory: never package local ROMs or saves.
stage="$repo_dir/dist/package"
rm -rf "$stage"
cmake --install build --prefix "$stage/PSP/GAME"
package="$stage/PSP/GAME/FrogN64"
rm -f "$package/daedalus.prx"
find "$package" -name .DS_Store -type f -delete
cp copying.txt "$package/COPYING.txt"

for required in EBOOT.PBP Plugins/mediaengine.prx Plugins/dvemgr.prx Plugins/exception.prx; do
    test -s "$package/$required" || {
        echo "Arquivo ausente no pacote: $required" >&2
        exit 1
    }
done

revision="$(git -c safe.directory="$repo_dir" rev-parse HEAD)"
tree_state=clean
if [[ -n "$(git -c safe.directory="$repo_dir" status --porcelain)" ]]; then
    tree_state=modified
fi
{
    printf 'Project: https://github.com/jussimir/FrogN64\n'
    printf 'Revision: %s\nWorking tree: %s\n' "$revision" "$tree_state"
    printf 'Built at (UTC): %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'Reference toolchain image: %s\n' "$image"
    psp-g++ --version | head -n 1
} > "$package/BUILD-INFO.txt"

archive="FrogN64-PSP-${revision:0:8}.zip"
if [[ "$tree_state" == modified ]]; then
    archive="FrogN64-PSP-${revision:0:8}-modified.zip"
fi
(
    cd "$stage"
    cmake -E tar cf "../$archive" --format=zip PSP
)
echo "Pacote gerado: dist/$archive"
