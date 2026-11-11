#!/usr/bin/env bash
# Build (if needed) and run AStarTileMap.
#   ./run.sh            -> asks for the map name in the terminal
#   ./run.sh dungeon    -> loads maps/dungeon.txt straight away
#   ./run.sh --clean    -> wipes build/ first, then builds and asks for a map
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

cd build   # the program loads Textures/ and maps/ relative to here
if [[ $# -ge 1 ]]; then
  echo "$1" | ./AStarTileMap
else
  ./AStarTileMap
fi
