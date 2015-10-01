#!/bin/bash
echo "Bench,Class,Type,Runid,Time" > results.csv
grep 'Time in seconds' -R [a-z][a-z].[A-Z] | sed -e \
    's/\([a-z][a-z]\)\.\([A-Z]\)\/run-\([0-9]*\)\/\([A-Z][a-z]*\).log:.*=[ ]*\([0-9]*\.[0-9]*\)/\1,\2,\4,\3,\5/'\
    >> results.csv
