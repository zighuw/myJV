#!/usr/bin/env bash
set +e

label="$1"
log="$2"
shift 2

"$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}

if [ $status -ne 0 ]; then
    message=$(tail -n 30 "$log" | tr -d '\r' | tr '\n' '|' | sed 's/%/%25/g')
    echo "::error::${label} failed: ${message}"
    exit $status
fi
