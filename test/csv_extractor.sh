#!/bin/bash
echo "Wakeup interval, run id, time (ms)" > results.csv
grep ms -R interval-* | sed -e \
    's/interval-\([0-9]*\)\/run-\([0-9]*\)\/.*:[->]* \([0-9]*\.[0-9]*\) .*/\1,\2,\3/'\
    >> results.csv
