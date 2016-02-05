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

if [ -z "$2" ]
then
    echo "Usage $0 PageSize TraceFile"
    exit 1
fi

DIR=$(dirname $0)
timer="$DIR/timer.sh"
TYPES=('Virtual' 'Physical')
parser="$DIR/framesoc_parser.pl"

# $1: type
# $2: pagesize
# $3: trace file
generate_producers()
{
    field=$(( $1 + 1 ))
    name=${TYPES[$1]}-producers.log
    echo "Generating ${TYPES[$1]} producers file"
    echo $2 > $name
    sed 1d $3 | cut -d , -f $field,8  | sort -u |\
        awk 'BEGIN{CUR=-1} {ADDR=sprintf("%s", $1);if(CUR==ADDR)\
        {TSKS=TSKS","$2}else{if(CUR!=-1){print CUR""TSKS};CUR=ADDR;TSKS=$2}}\
            END{print CUR""TSKS}' >> $name
    echo "${TYPES[$1]}-producers.log Done"
}

t=$($timer)
echo "Generating producer files"
generate_producers 0 $1 $2 &
pid0=$!
generate_producers 1 $1 $2 &
pid1=$!
wait $pid0
wait $pid1
$timer $t
t=$($timer)
echo "Generating framesoc trace file"
cd $(dirname $2)
$DIR/$parser -v -p $1 -i $(basename $2)
$timer $t
