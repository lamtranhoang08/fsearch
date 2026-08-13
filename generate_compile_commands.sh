#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_FILE="$ROOT_DIR/compile_commands.json"

SOURCES=(
    "src/tokenizer.cpp"
    "src/crawler.cpp"
    "src/inverted_index.cpp"
    "src/main.cpp"
)

{
    echo "["
    for i in "${!SOURCES[@]}"; do
        src="${SOURCES[$i]}"
        comma=","
        if [[ $i -eq $((${#SOURCES[@]} - 1)) ]]; then
            comma=""
        fi
        cat <<EOF
  {
    "directory": "$ROOT_DIR",
    "command": "g++ -std=c++17 -Wall -Wextra -Iinclude -c $src",
    "file": "$ROOT_DIR/$src"
  }$comma
EOF
    done
    echo "]"
} > "$OUT_FILE"

echo "Wrote $OUT_FILE"