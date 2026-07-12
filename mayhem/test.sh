#!/usr/bin/env bash
#
# mayhem/test.sh — RUN libnest2d's own upstream test suite (already built by mayhem/build.sh).
#
# The suite is upstream's tests/test.cpp (Catch2, the same tests_clipper_nlopt executable that
# upstream's tests/CMakeLists.txt defines) — behavioral known-answer/assertion tests over the
# geometry, nesting, placement and optimizer code. build.sh compiled it with normal flags to
# /mayhem/tests_clipper_nlopt; this script only runs it and reports CTRF counts.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER=/mayhem/tests_clipper_nlopt
if [ ! -x "$RUNNER" ]; then
  echo "FATAL: $RUNNER missing — mayhem/build.sh should have built it" >&2
  emit_ctrf "catch2" 0 1
  exit 1
fi

# Run from /tmp: some tests write SVG scratch output to the CWD.
out="$(cd /tmp && "$RUNNER" 2>&1)"; rc=$?
echo "$out" | tail -20

passed=0; failed=0
# Catch2 v2 console summary:
#   all green:   "All tests passed (N assertions in M test cases)"
#   otherwise:   "test cases: T | P passed | F failed"
if summary="$(echo "$out" | grep -E '^All tests passed \([0-9]+ assertions in [0-9]+ test cases?\)')" && [ -n "$summary" ]; then
  passed="$(echo "$summary" | sed -E 's/.* in ([0-9]+) test cases?\).*/\1/')"
  failed=0
elif summary="$(echo "$out" | grep -E '^test cases:')" && [ -n "$summary" ]; then
  passed="$(echo "$summary" | grep -oE '[0-9]+ passed' | grep -oE '[0-9]+' || echo 0)"
  failed="$(echo "$summary" | grep -oE '[0-9]+ failed' | grep -oE '[0-9]+' || echo 0)"
else
  echo "FATAL: could not parse Catch2 summary (runner exit=$rc)" >&2
  emit_ctrf "catch2" 0 1
  exit 1
fi
# A crashed runner (no summary handled above); if it exited nonzero but reported 0 failed, distrust it.
if [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi

emit_ctrf "catch2" "$passed" "$failed"
