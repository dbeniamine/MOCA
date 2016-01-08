#!/bin/bash

# Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
# Author: Beniamine, David <David@Beniamine.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

WD=$PWD
echo "Bench,Run,Type,Pages,Address,Accesses" > $WD/events.csv
for f in *.A
do
    cd $f
    echo $f
    for ff in run-*
    do
        cd $ff
        id=${ff/run-/}
        echo $id
        # Tabarnac values
        if [ -e "$f.full.page.csv" ]
        then
            echo "pin"
            pages=$(sed 1d $f.full.page.csv | cut -d ',' -f 1 | sort -nu | wc -l)
            nacc=$(sed 1d $f.full.page.csv | cut -d ',' -f 4- | sed 's/,/\n/g' \
                | awk 'BEGIN{s=0}{s+=$1}END{print s}')
            echo "$f,$id,Pin,$pages,$pages,$nacc" >> $WD/events.csv
            echo "done"
        fi
        # Moca values
        for tr in $(find . -name Moca-*.csv)
        do
            name=$(dirname $tr | sed 's/.\/\([^-]*\)-.*/\1/')
            echo $name
            #trace=$(sed '1d' $tr | cut -d ',' -f 1,3,4 | tr '\n' 'N')
            addr=$(sed '1d' $tr | cut -d ',' -f 1 | sort -u | wc -l)
            pages=$(sed '1d' $tr | cut -d ',' -f 1 | \
                sed 's/\(.*\)[a-f0-9]\{3\}/\1/' | sort -u | wc -l)
            nacc=$(sed '1d' $tr | cut -d ',' -f 3,4 | tr ',' ' ' \
                | awk 'BEGIN{s=0}{s+=$1+$2}END{print s}')
            #unset trace
            echo "$f,$id,$name,$pages,$addr,$nacc" >> $WD/events.csv
            echo "done"
        done
        # Mitos stuff
        for tr in $(find . -name "samples.csv")
        do
            # mitos todo: differentiate mitos run
            echo "mitos"
            trace=$(sed 1d $tr | cut -d ',' -f 15 | sort -nu | tr '\n' 'N' )
            addr=$(echo $trace | tr 'N' '\n' | wc -l)
            pages=$(echo $trace | tr 'N' '\n' | awk '{printf("%x\n", $1)}'\
                 | sed 's/\(.*\)[a-f0-9]\{3\}/\1/' | sort -u | wc -l)
            unset trace
            nacc=$addr
            echo "$f,$id,Mitos,$pages,$addr,$nacc" >> $WD/events.csv
            echo "done"
        done
        cd ..
    done
    cd ..
done
