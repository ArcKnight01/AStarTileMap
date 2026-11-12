#!/usr/bin/env bash
# Build (if needed) and run AStarTileMap. It opens on the menu.
#   ./run.sh            -> menu with an empty map name (START loads "default")
#   ./run.sh dungeon    -> menu with "dungeon" already typed in the map name box
#   ./run.sh --clean    -> wipes build/ first, then builds and runs
set -euo pipefail

cd "$(dirname "$0")"

if [[ "${1:-}" == "--clean" ]]; then
  rm -rf ./build
  shift
fi

if ! command -v cmake >/dev/null; then
  echo "cmake not found. Install with: sudo apt install build-essential cmake libsfml-dev" >&2
  exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build build -j"$(nproc)"

cd build   # the program loads Textures/, Font/ and maps/ relative to here
./AStarTileMap "$@"
