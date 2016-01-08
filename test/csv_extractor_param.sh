#!/bin/bash
echo "Bench,Class,Monitor,Log,Runid,Time" > results.csv
grep 'Time in seconds' -R mon-* | sed -e \
    's/.*mon-\([0-9]*\)\/log-\([0-9]*\)\/run-\([0-9]*\)\/.*Moca-\([a-z][a-z]\)\.\([A-Z]\).log:.*=[ ]*\([0-9]*\.[0-9]*\)/\4,\5,\1,0.\2,\3,\6/'\
    >> results.csv
