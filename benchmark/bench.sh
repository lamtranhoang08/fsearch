#!/usr/bin/env bash
# Benchmarks fsearch (pre-built index) against a naive `grep -r` scan
# over the same directory, for a repeated query workload.
#
# Usage: ./bench.sh <target_dir> <query_word> [num_queries]

set -euo pipefail

TARGET_DIR="${1:?Usage: bench.sh <target_dir> <query_word> [num_queries]}"
QUERY="${2:?Usage: bench.sh <target_dir> <query_word> [num_queries]}"
NUM_QUERIES="${3:-20}"

FSEARCH_BIN="$(dirname "$0")/../fsearch"
INDEX_FILE="/tmp/bench_$$.idx"

if [[ ! -x "$FSEARCH_BIN" ]]; then
    echo "Build fsearch first: run 'make' in the project root." >&2
    exit 1
fi

echo "== Building index for $TARGET_DIR =="
BUILD_START=$(date +%s.%N)
"$FSEARCH_BIN" build "$TARGET_DIR" "$INDEX_FILE"
BUILD_END=$(date +%s.%N)
echo "Index build time: $(echo "$BUILD_END - $BUILD_START" | bc)s"
echo

echo "== Running $NUM_QUERIES queries for '$QUERY' =="

FSEARCH_START=$(date +%s.%N)
for _ in $(seq 1 "$NUM_QUERIES"); do
    "$FSEARCH_BIN" search "$INDEX_FILE" "$QUERY" > /dev/null
done
FSEARCH_END=$(date +%s.%N)
FSEARCH_TOTAL=$(echo "$FSEARCH_END - $FSEARCH_START" | bc)

GREP_START=$(date +%s.%N)
for _ in $(seq 1 "$NUM_QUERIES"); do
    grep -r -l "$QUERY" "$TARGET_DIR" > /dev/null 2>&1 || true
done
GREP_END=$(date +%s.%N)
GREP_TOTAL=$(echo "$GREP_END - $GREP_START" | bc)

echo
printf "%-20s %10s %15s\n" "Method" "Total (s)" "Avg/query (ms)"
printf "%-20s %10.4f %15.4f\n" "fsearch (indexed)" "$FSEARCH_TOTAL" "$(echo "$FSEARCH_TOTAL * 1000 / $NUM_QUERIES" | bc -l)"
printf "%-20s %10.4f %15.4f\n" "grep -r (naive)" "$GREP_TOTAL" "$(echo "$GREP_TOTAL * 1000 / $NUM_QUERIES" | bc -l)"

rm -f "$INDEX_FILE"
