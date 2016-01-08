#!/bin/bash
WD=$PWD
bench=mg
echo "Bench,Monitor,Log,Run,Nacc" > $WD/events.csv
for dir in $(find . -name Moca-$bench)
do
    cd $dir
    acc=$(wc -l Moca-full-trace.csv | cut -f 1 -d ' ')
    line=$(echo $dir | sed \
        's/\.\/mon-\([0-9]*\)\/log-\([0-9]*\)\/run-\([0-9]*\)\/.*/\1,0.\2,\3/')
    echo "$bench,$line,$acc" >> $WD/events.csv
    cd $WD
done
