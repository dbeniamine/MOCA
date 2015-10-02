#!/bin/bash
WD=$PWD
echo "Bench,Run,Type,Pages,Accesses" > $WD/events.csv
for f in *.A
do cd $f
    echo $f
    for ff in run-*
    do cd $ff
        id=${ff/run-/}
        echo $id
        # Unarchive traces
        #tar xvJf traces.tar.xz
        # Tabarnac values
        pages=$(sed 1d $f.full.page.csv | cut -d ',' -f 1 | sort -nu | wc -l)
        nacc=$(sed 1d $f.full.page.csv | cut -d ',' -f 4- | sed 's/,/\n/g' \
            | awk 'BEGIN{s=0}{s+=$1}END{print s}')
        echo "$f,$id,Pin,$pages,$nacc" >> $WD/events.csv
        # Moca values
        pages=$(find . -name Moca-task*.log | xargs cat | grep ^A \
            | sed 's/\(.*\)[a-f0-9]\{3\}/\1/' | awk '{print $2}' | sort -u | wc -l)
        nacc=$(find . -name Moca-task*.log | xargs cat | grep ^A \
            | awk 'BEGIN{s=0}{s+=$4+$5}END{print s}')
        echo "$f,$id,Moca,$pages,$nacc" >> $WD/events.csv
        cd ..
    done
    cd ..
done
