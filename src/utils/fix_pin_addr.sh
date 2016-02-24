#!/bin/bash

# Copyright (C) 2016  Beniamine, David <David@Beniamine.net>
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

decimal(){
    if [ -z "$(echo $1 | grep 0x)" ]
    then
        num="0x$1"
    else
        num=$1
    fi
    printf "%lu\n" $num
}

timer=$(dirname $0)/timer.sh
program=$1
lim=$(decimal $2)

t=$($timer)
echo "Fixing high malloc addresses"
# Get actual addresses
ACTUAL_ADDR=($(grep "Moca high" Moca-$program.log | sed 's/^[^0-9]*//' | sort -n))
cpt=0
# Sort the current struct file by addressses
head -n 1 $program.structs.csv > temp_$$
sed 1d $program.structs.csv | awk -F , '{print $2","$1","$3}' | sort -n | \
    awk -F , '{print $2","$1","$3}' >> temp
mv temp $program.structs.csv
# Do replace
tempf="$program.structs.temp_$$.csv"
echo "" > $tempf
while read line
do
    array=(${line//,/ })
    if [[ ${array[1]} -gt $lim ]]
    then
        # Replace bad pin addr by actual malloc addr
        echo "${array[0]},${ACTUAL_ADDR[$cpt]},${array[2]}" >> $tempf
        cpt=$(( $cpt +1 ))
    else
        echo $line >> $tempf
    fi
done < $program.structs.csv
mv $tempf $program.structs.csv

$timer $t
t=$($timer)
echo "Generating $program-stackmap.csv"
cpt=0
echo "tid,stackmax,sz" > $program-stackmap.csv
grep 'stack:' stacks.log | cut -f 1 | while read line
do
    array=(${line//-/ })
    addr=$(decimal ${array[0]})
    sz=$(( $(decimal ${array[1]}) - $addr ))
    echo "$cpt,$addr,$sz" >> $program-stackmap.csv
    cpt=$(($cpt+1))
done
$timer $t
