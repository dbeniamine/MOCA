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

START_TIME=$(date +%y%m%d_%H%M%S)
CMDLINE="$0 $@"
EXP_NAME=$(basename $0 .sh)
OUTPUT="exp.log"
OWNER=david
RUN=5
CONFIGS=('base' 'moca' )
PREFIX=/home/$OWNER/Work
NAS="$PREFIX/Bench/NPB3.3-OMP/"
MOCAPATH="$PREFIX/Moca/"
declare -A TARGET
TARGET=([base]="" [moca]="$MOCAPATH/src/utils/moca -d $MOCAPATH -G -c ")
#report error if needed
function testAndExitOnError
{
    err=$?
    if [ $err -ne 0 ]
    then
        echo "ERROR $err : $1"
        exit $err
    fi
}
function dumpInfos
{

    #Echo start time
    echo "Expe started at $START_TIME"
    #Echo args
    echo "#### Cmd line args : ###"
    echo "$CMDLINE"
    echo "EXP_NAME $EXP_NAME"
    echo "OUTPUT $OUTPUT"
    echo "RUN $RUN"
    echo "########################"
    # DUMP environement important stuff
    echo "#### Hostname: #########"
    hostname
    echo "#### Path:     #########"
    echo "$PATH"
    echo "########################"
    echo "##### git log: #########"
    git log | head
    echo "########################"
    echo "#### git diff: #########"
    git diff
    echo "########################"
    lstopo --of txt
    cat /proc/cpuinfo
    echo "########################"


    #DUMPING scripts
    cp -v $0 $EXP_DIR/
    cp -v ./*.sh $EXP_DIR/
    cp -v *.pl $EXP_DIR/
    cp -v *.rmd  $EXP_DIR/
    cp -v Makefile  $EXP_DIR/
}
if [ $(whoami) != "root" ]
then
    echo "This script must be run as root"
    exit 1
fi
#parsing args
while getopts "ho:e:r:" opt
do
    case $opt in
        h)
            usage
            exit 0
            ;;
        e)
            EXP_NAME=$OPTARG
            ;;
        o)
            OUTPUT=$OPTARG
            ;;
        r)
            RUN=$OPTARG
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
#post init
EXP_DIR="$PWD/$EXP_NAME"_$(date +%y%m%d_%H%M)
mkdir $EXP_DIR
OUTPUT="$EXP_DIR/$OUTPUT"

#Continue but change the OUTPUT
exec > >(tee $OUTPUT) 2>&1
dumpInfos

#Do the first compilation
cd $NAS
make clean
make suite
rm bin/*.x
cd -


for run in $(seq 1 $RUN)
do
    echo "RUN : $run"
    #Actual exp
    for bench in $(\ls $NAS/bin)
    do
        echo "$bench"
        LOGDIR="$EXP_DIR/$bench/run-$run"
        mkdir -p $LOGDIR
        #Actual experiment
        for conf in ${CONFIGS[@]}
        do
            if [ $conf == "moca" ]
            then
                MOCA_LOGDIR="$EXP_DIR/$bench/run-$run/Moca-$bench"
                cmd="$MOCAPATH/src/utils/moca -d $MOCAPATH -G -D $MOCA_LOGDIR -c"
            else
                cmd=""
            fi
            set -x
            echo $cmd $NAS/bin/$bench #> $LOGDIR/$conf.log 2> $LOGDIR/$conf.err
            set +x
            testAndExitOnError "run number $run"
        done
    done
done

#cd $EXP_DIR/
#./parseAndPlot.sh
#cd -
#Echo thermal throttle info
echo "thermal_throttle infos :"
cat /sys/devices/system/cpu/cpu0/thermal_throttle/*
END_TIME=$(date +%y%m%d_%H%M%S)
echo "Expe ended at $END_TIME"
chown -R $OWNER:$OWNER $EXP_DIR
