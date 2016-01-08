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
OWNER=dbeniamine
RUN=30
PREFIX="/home/dbeniamine"
WORKPATH="/tmp"
NAS="NPB3.3-OMP/"
MOCAPATH="Moca"
BENCH=mg
CLASS=A

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
    echo "FIRST: $FIRSTRUN"
    echo "LAST: $LASTRUN"
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
id=0
#parsing args
while getopts "ho:e:r:i:" opt
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
        i)
            id=$OPTARG
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
if [ $(whoami) != "root" ]
then
    echo "This script must be run as root"
    exit 1
fi

#post init
EXP_DIR="$WORKPATH/$EXP_NAME"_$(date +%y%m%d_%H%M)
mkdir $EXP_DIR
OUTPUT="$EXP_DIR/$OUTPUT"
FIRSTRUN=$(( $id * $RUN ))
LASTRUN=$(( $FIRSTRUN + $RUN ))
FIRSTRUN=$(( $FIRSTRUN +1 ))

#Continue but change the OUTPUT
exec > >(tee $OUTPUT) 2>&1
dumpInfos

# Copy files to working dir
if [ $PREFIX != $WORKPATH ]
then
    cp -rv $PREFIX/$NAS $WORKPATH/
    cp -rv $PREFIX/$MOCAPATH $WORKPATH/
fi

#Do the first compilation
cd $WORKPATH/$NAS
make clean
make $BENCH CLASS=$CLASS
rm bin/*.x
cd -

bench="$WORKPATH/$NAS/bin/$BENCH.$CLASS"
cd $WORKPATH/$MOCAPATH/src
make clean
make
cd -

for run in $(seq $FIRSTRUN $LASTRUN)
do
    echo "RUN : $run"
    #Actual exp
    for wint in $(seq 20 10 100 | shuf) #ms
    do
        for lint in $(seq 1 9 | shuf) #sec
        do
            LOGDIR="$EXP_DIR/mon-$wint/log-$lint/run-$run"
            mkdir -p $LOGDIR
            echo $LOGDIR
	        cmd="$WORKPATH/$MOCAPATH/src/utils/moca -w $wint -L .$lint \
                -D $LOGDIR/Moca-$BENCH -c $bench"
            #Actual experiment
            set -x
            $cmd > $LOGDIR/log 2> $LOGDIR/err
            #testAndExitOnError "Exec failed $conf $BENCH run-$run"
            set +x
        done
        #echo "Compressing traces"
        #tar cvJf $LOGDIR/traces.tar.xz $LOGDIR/Moca-$BENCH *.csv
        #rm -rf $LOGDIR/Moca-$BENCH *.csv
        #echo "Done"
    done
    echo "Saving files"
	sudo chmod -R 777 $EXP_DIR
	chown -R $OWNER: $EXP_DIR
	su $OWNER -c "cp -ur $EXP_DIR /home/$OWNER/"
    echo "Done"
done

#cd $EXP_DIR/
#./parseAndPlot.sh
#cd -
#Echo thermal throttle info
echo "retrieving expe files"
sudo chmod -R 777 $EXP_DIR
chown -R $OWNER: $EXP_DIR
su $OWNER -c "cp -ur $EXP_DIR /home/$OWNER/"
echo "thermal_throttle infos :"
cat /sys/devices/system/cpu/cpu0/thermal_throttle/*
END_TIME=$(date +%y%m%d_%H%M%S)
echo "Expe ended at $END_TIME"
