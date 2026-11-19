#!/usr/bin/env bash
# Scanner regression tests
# Usage: scanner_tests/run.sh [-v]   (-v: valgrind)
# Prefix: ok_  -> lexically valid: 0, syntax (2), or semantic (3-7, 10)
#         e1_  -> lexical error: 1
#         xok_ -> lexically valid by the spec, not supported yet
#         xe1_ -> should fail with 1, not enforced yet
set -u
shopt -s nullglob

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
BIN="$ROOT/compiler"
TIMEOUT_SEC=5      # hang = fail
VG_ERR=125         # valgrind error exit code
USE_VG=0
[ "${1:-}" = "-v" ] && USE_VG=1
VG_TIMEOUT_SEC=120  # valgrind is slow; first run may download glibc debuginfo

# Warn about slow valgrind mode
if [ "$USE_VG" -eq 1 ]; then
    echo "valgrind mode: ~1s per test; first run may download glibc debuginfo (debuginfod)"
    if ! valgrind -q true >/dev/null 2>&1; then
        echo "VALGRIND BROKEN (check DEBUGINFOD_URLS / network)"; exit 1
    fi
fi

files=("$DIR"/cases/*.ifj)
total=${#files[@]}
if [ "$total" -eq 0 ]; then
    echo "NO TEST FILES in $DIR/cases/*.ifj"; exit 1
fi
i=0

# Build compiler
make -s -C "$ROOT" >/dev/null || { echo "BUILD FAILED"; exit 1; }

# Expected code from file prefix
expected_code() {
    case "$1" in
        ok_*|xok_*) echo "0/2-7/10" ;;
        e1_*|xe1_*) echo 1 ;;
        *)          echo -1 ;;
    esac
}

# Any exit code except 1 (lexical error) and 99 (internal error) means lexer succeeded
lexical_ok() {
    case "$1" in
        0|2|3|4|5|6|7|10) return 0 ;;
        *)                return 1 ;;
    esac
}

pass=0; fail=0; xfail=0; xpass=0

for f in "${files[@]}"; do
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
        if lexical_ok "$got"; then
            [ "$USE_VG" -eq 1 ] && printf '\n'
            echo "XPASS $name (rename to ok_)"; xpass=$((xpass+1))
        else
            xfail=$((xfail+1))
        fi
        continue
    fi
    if [[ "$name" == xe1_* ]]; then
        if [ "$got" -eq 1 ]; then
            [ "$USE_VG" -eq 1 ] && printf '\n'
            echo "XPASS $name (rename to e1_)"; xpass=$((xpass+1))
        else
            xfail=$((xfail+1))
        fi
        continue
    fi

    # ok_ accepts any code without a lexical error, e1_ needs exact code 1
    if [[ "$name" == ok_* ]]; then
        lexical_ok "$got" && good=1 || good=0
    else
        [ "$got" -eq "$exp" ] && good=1 || good=0
    fi
    if [ "$good" -eq 0 ]; then
        [ "$got" -eq 124 ] && got="TIMEOUT"
        [ "$USE_VG" -eq 1 ] && printf '\n'
        echo "FAIL  $name: expected $exp, got $got"; fail=$((fail+1)); continue
    fi

    # Optional memory check
    if [ "$USE_VG" -eq 1 ]; then
        timeout "$VG_TIMEOUT_SEC" valgrind -q --leak-check=full \
            --errors-for-leak-kinds=definite,indirect \
            --error-exitcode=$VG_ERR "$BIN" < "$f" >/dev/null 2>&1
        vg=$?
        if [ "$vg" -ne "$got" ]; then
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