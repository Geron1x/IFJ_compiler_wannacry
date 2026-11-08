#!/usr/bin/env bash
# Parser regression tests
# Usage: parser_tests/run.sh [-v]   (-v: valgrind)
# Prefix: ok_ -> 0, e1_ -> 1, e2_ -> 2, xok_ -> 0 but not supported yet
set -u

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
BIN="$ROOT/compiler"
TIMEOUT_SEC=5      # hang = fail
VG_ERR=125         # valgrind error exit code
USE_VG=0
[ "${1:-}" = "-v" ] && USE_VG=1
VG_TIMEOUT_SEC=120  # valgrind is slow; first run downloads glibc debuginfo

# Warn about slow valgrind mode
if [ "$USE_VG" -eq 1 ]; then
    echo "valgrind mode: ~1s per test; first run may download glibc debuginfo (debuginfod)"
    # Sanity check that valgrind works at all
    if ! valgrind -q true >/dev/null 2>&1; then
        echo "VALGRIND BROKEN (check DEBUGINFOD_URLS / network)"; exit 1
    fi
fi

total=$(ls "$DIR"/cases/*.ifj | wc -l)
i=0

# Build compiler
make -s -C "$ROOT" >/dev/null || { echo "BUILD FAILED"; exit 1; }

# Expected code from file prefix
expected_code() {
    case "$1" in
        ok_*|xok_*) echo 0 ;;
        e1_*)       echo 1 ;;
        e2_*)       echo 2 ;;
        *)          echo -1 ;;
    esac
}

pass=0; fail=0; xfail=0; xpass=0

for f in "$DIR"/cases/*.ifj; do
    name="$(basename "$f" .ifj)"
    exp="$(expected_code "$name")"
    i=$((i+1))
    # Progress line in valgrind mode
    [ "$USE_VG" -eq 1 ] && printf '\r[%d/%d] %-40s' "$i" "$total" "$name"

    # Run with timeout to catch hangs
    timeout "$TIMEOUT_SEC" "$BIN" < "$f" >/dev/null 2>&1
    got=$?

    # Known-unsupported tests
    if [[ "$name" == xok_* ]]; then
        if [ "$got" -eq 0 ]; then
            echo "XPASS $name (rename to ok_)"; xpass=$((xpass+1))
        else
            xfail=$((xfail+1))
        fi
        continue
    fi

    # Compare exit code
    if [ "$got" -ne "$exp" ]; then
        [ "$got" -eq 124 ] && got="TIMEOUT"
        echo "FAIL  $name: expected $exp, got $got"; fail=$((fail+1)); continue
    fi

    # Optional memory check
    if [ "$USE_VG" -eq 1 ]; then
        timeout "$VG_TIMEOUT_SEC" valgrind -q --leak-check=full \
            --errors-for-leak-kinds=definite,indirect \
            --error-exitcode=$VG_ERR "$BIN" < "$f" >/dev/null 2>&1
        vg=$?
        # Valgrind must return the program's own code
        if [ "$vg" -ne "$exp" ]; then
            case "$vg" in
                "$VG_ERR") msg="LEAK/memory error" ;;
                124)       msg="valgrind TIMEOUT" ;;
                *)         msg="valgrind failed (exit $vg)" ;;
            esac
            printf '\n'; echo "FAIL  $name: $msg"; fail=$((fail+1)); continue
        fi
    fi
    pass=$((pass+1))
done

# Summary
[ "$USE_VG" -eq 1 ] && printf '\n'
echo "pass=$pass fail=$fail xfail=$xfail xpass=$xpass"
[ "$fail" -eq 0 ]
