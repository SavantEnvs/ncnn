#!/usr/bin/env bash
# mayhem/test.sh — RUN ncnn's own unit tests (built by build.sh with normal flags) plus
# known-answer checks on upstream's bundled SqueezeNet model. Emits CTRF.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"
TS="$SRC/build-tests"

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

if [ ! -f "$TS/mayhem-tests.txt" ] || [ ! -x "$TS/kat_squeezenet" ]; then
  echo "test build missing — run mayhem/build.sh first" >&2
  emit_ctrf "ncnn-ctest" 0 1
  exit 1
fi

passed=0 failed=0
# 1) upstream unit tests (each asserts its own results; ctest maps a non-zero exit to failure)
while read -r t; do
  [ -n "$t" ] || continue
  if [ -x "$TS/tests/$t" ] && (cd "$TS/tests" && "./$t" >/dev/null 2>&1); then
    passed=$((passed + 1))
  else
    echo "FAIL: $t"; failed=$((failed + 1))
  fi
done < "$TS/mayhem-tests.txt"

# 2) known answers: SqueezeNet v1.1 classification through the text and binary param loaders
want_kat=$(cat "$SRC/mayhem/testdata/kat_squeezenet.expected")
got_kat=$("$TS/kat_squeezenet" examples/squeezenet_v1.1.param examples/squeezenet_v1.1.param.bin examples/squeezenet_v1.1.bin 2>&1)
if [ "$got_kat" = "$want_kat" ]; then passed=$((passed + 1)); else
  echo "FAIL: kat_squeezenet"; echo " want: $want_kat"; echo " got:  $got_kat"; failed=$((failed + 1)); fi

# 3) known answers: ncnn2mem output for the bundled SqueezeNet param must match the shipped .param.bin
tmp=$(mktemp -d)
cp examples/squeezenet_v1.1.param "$tmp/sq.param"   # ncnn2mem writes <param>.bin next to its input
if "$TS/tools/ncnn2mem" "$tmp/sq.param" examples/squeezenet_v1.1.bin "$tmp/id.h" "$tmp/mem.h" >/dev/null 2>&1 \
   && (cd "$tmp" && sha256sum id.h sq.param.bin) | diff - "$SRC/mayhem/testdata/ncnn2mem.sha256" >/dev/null; then
  passed=$((passed + 1)); else echo "FAIL: ncnn2mem output"; failed=$((failed + 1)); fi
rm -rf "$tmp"

emit_ctrf "ncnn-unit+kat" "$passed" "$failed"
