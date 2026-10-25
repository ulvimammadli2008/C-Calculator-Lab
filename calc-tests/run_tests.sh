#!/bin/sh
# Runs calc on every tests/NAME.txt and compares with tests/NAME_output.txt.
#   sh run_tests.sh [path-to-calc]        default: ./calc
# Build first:  gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

CALC=${1:-./calc}
DIR=$(dirname "$0")/tests
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
pass=0; fail=0

for in in "$DIR"/*.txt; do
    case "$in" in *_output.txt) continue ;; esac
    exp="${in%.txt}_output.txt"
    [ -f "$exp" ] || continue
    name=$(basename "$in" .txt)
    "$CALC" -o "$OUT/$name" "$in" 2>/dev/null
    got=$(cat "$OUT/$name"/*.txt 2>/dev/null)
    want=$(cat "$exp")
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL  $name"
        echo "      input:    $(head -c 60 "$in" | tr '\n' '|')"
        echo "      expected: $(echo "$want" | tr '\n' ' ')"
        echo "      got:      $(echo "$got" | tr '\n' ' ')"
    fi
done

echo
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
