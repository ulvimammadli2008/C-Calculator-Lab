#!/bin/sh
# Command-line tests for calc (grade 9).
#   sh cli.sh [path-to-calc]        default: ./calc
# Prints PASS/FAIL per test; exit code 1 if anything failed.

CALC=${1:-./calc}
case "$CALC" in /*) ;; *) CALC="$(pwd)/$CALC" ;; esac
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fails=0

pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1: $2"; fails=$((fails + 1)); }

# one result file named <base>_<name>_<lastname>_<id>.txt in folder $2 for base $1
result_in() {
    set -- "$2"/"$1"_*_*_*.txt
    [ -f "$1" ] && [ $# -eq 1 ] && cat "$1"
}

mkdir -p "$T/in/sub" "$T/run"
printf '2 + 3\n' > "$T/in/a.txt"
printf '10 - 4\n' > "$T/in/b.txt"
printf '1 + 1\n' > "$T/in/notes.md"
printf '7 - 7\n' > "$T/in/sub/c.txt"
printf '1 + * 2\n' > "$T/in/bad.txt"
cd "$T/run" || exit 1

# --- -o with a single file -------------------------------------------------
"$CALC" -o out1 ../in/a.txt >/dev/null 2>&1
if [ "$(result_in a out1)" = "5" ]; then pass g9_cli_o_short; else fail g9_cli_o_short "expected out1/a_<name>_<lastname>_<id>.txt containing 5"; fi

"$CALC" --output-dir out2 ../in/b.txt >/dev/null 2>&1
if [ "$(result_in b out2)" = "6" ]; then pass g9_cli_o_long; else fail g9_cli_o_long "--output-dir did not write b_*.txt containing 6"; fi

# --- -d: only *.txt directly inside the folder ------------------------------
"$CALC" -d ../in -o out3 >/dev/null 2>&1
n=$(ls out3 2>/dev/null | wc -l | tr -d ' ')
if [ "$(result_in a out3)" = "5" ] && [ "$(result_in b out3)" = "6" ] && [ "$(result_in bad out3)" = "ERROR:5" ] && [ "$n" = "3" ]; then
    pass g9_cli_dir_short
else
    fail g9_cli_dir_short "expected exactly 3 result files (a, b, bad); .md files and subfolders must be ignored; found $n"
fi

"$CALC" --dir ../in --output-dir out4 >/dev/null 2>&1
if [ "$(result_in a out4)" = "5" ] && [ "$(ls out4 2>/dev/null | wc -l | tr -d ' ')" = "3" ]; then pass g9_cli_dir_long; else fail g9_cli_dir_long "--dir with --output-dir"; fi

# --- default output folder: <input_base>_<name>_<id> in the current folder --
"$CALC" ../in/a.txt >/dev/null 2>&1
set -- a_*_*/
if [ -d "$1" ] && [ $# -eq 1 ] && [ "$(result_in a "${1%/}")" = "5" ]; then pass g9_cli_default_file; else fail g9_cli_default_file "expected folder a_<name>_<id>/ with a_*.txt"; fi

"$CALC" -d ../in >/dev/null 2>&1
set -- in_*_*/
if [ -d "$1" ] && [ $# -eq 1 ] && [ "$(result_in b "${1%/}")" = "6" ]; then pass g9_cli_default_dir; else fail g9_cli_default_dir "expected folder in_<name>_<id>/ for -d ../in"; fi

# --- existing output folder is fine ------------------------------------------
"$CALC" -o out1 ../in/b.txt >/dev/null 2>&1
if [ "$(result_in b out1)" = "6" ]; then pass g9_cli_existing_dir; else fail g9_cli_existing_dir "writing into an existing output folder failed"; fi

# --- exit codes and error messages ---------------------------------------------
"$CALC" -o out5 ../in/bad.txt >/dev/null 2>&1; rc=$?
if [ $rc -eq 0 ] && [ "$(result_in bad out5)" = "ERROR:5" ]; then pass g9_cli_error_result_exit0; else fail g9_cli_error_result_exit0 "an ERROR result is not a program failure: exit code must be 0 (got $rc)"; fi

"$CALC" -o out6 ../in/missing.txt >/dev/null 2>"$T/err"; rc=$?
if [ $rc -ne 0 ] && [ -s "$T/err" ]; then pass g9_cli_missing_file; else fail g9_cli_missing_file "missing input file: need a message on stderr and a non-zero exit code (got $rc)"; fi

"$CALC" >/dev/null 2>"$T/err"; rc=$?
if [ $rc -ne 0 ] && [ -s "$T/err" ]; then pass g9_cli_no_args; else fail g9_cli_no_args "no arguments: print usage to stderr and exit non-zero (got $rc)"; fi

"$CALC" -x ../in/a.txt >/dev/null 2>"$T/err"; rc=$?
if [ $rc -ne 0 ] && [ -s "$T/err" ]; then pass g9_cli_unknown_option; else fail g9_cli_unknown_option "unknown option: message on stderr and non-zero exit (got $rc)"; fi

"$CALC" -d ../in/nosuchdir -o out7 >/dev/null 2>"$T/err"; rc=$?
if [ $rc -ne 0 ] && [ -s "$T/err" ]; then pass g9_cli_missing_dir; else fail g9_cli_missing_dir "missing -d folder: message on stderr and non-zero exit (got $rc)"; fi

echo
[ $fails -eq 0 ] && echo "cli: all passed" || echo "cli: $fails failed"
[ $fails -eq 0 ]
