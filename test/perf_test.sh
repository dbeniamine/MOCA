#!/bin/bash
INTERVALS=('20' '0' '30' '40' '50' '60' '70' '80' '90' '100' '110')
START_TIME=$(date +%y%m%d_%H%M%S)
CMDLINE="$0 $@"
EXP_NAME=$(basename $0)
OUTPUT="log"
RUN=30
COMPI="-n"
SEED=1550
ALGO=par_modulo
VERIF=""
THREADS=2
SIZE=1000
NBCHUNKS=40
TARGET=matrix
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
    echo "OUTPUT: $OUTPUT"
    echo "RUN $RUN"
    echo "Owner of the outputs : $OUTPUT_USER"
    echo "########################"
    # DUMP environement important stuff
    echo "#### Hostname: #########"
    hostname
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
EXP_DIR="$EXP_NAME"_$(date +%y%m%d_%H%M)
mkdir $EXP_DIR
OUTPUT="$EXP_DIR/$OUTPUT"

#Do the first compilation
cd ../src/module
make
cd -

#Continue but change the OUTPUT
exec > >(tee $OUTPUT) 2>&1
dumpInfos



for run in $(seq 1 $RUN)
do
    echo "RUN : $run"
    #Actual exp
    for int in ${INTERVALS[@]}
    do
        echo "Wakeup Interval: $int"
        LOGDIR="$EXP_DIR/interval-$int/run-$run"
        mkdir -p $LOGDIR
        #Actual experiment
        free -h
        if [ $int -ne 0 ]
        then
            EXECCMD="make COMPI=$COMPI LOGDIR=$LOGDIR SEED=$SEED SIZE=$SIZE \
                ALGO=$ALGO THREADS=$THREADS VERIF=$VERIF INTERVAL=$int \
                LOG_FILE=$LOGDIR/app-log INTERVAL=$int NB_CHUNKS=$NBCHUNKS \
                $TARGET"
                EXEC_LOG=make-log
        else
            EXECCMD="matrix/matrix -S $SIZE -s $SEED -a $ALGO -n $THREADS"
            EXEC_LOG=app-log
        fi
        echo "$EXECCMD"
        $EXECCMD > $LOGDIR/$EXEC_LOG
        testAndExitOnError "run number $run"
        free -h
    done
done

if [ ! -z "$OUTPUT_USER" ]
then
    chown -R "$OUTPUT_USER":"$OUTPUT_USER" $EXP_DIR
fi
#cd $EXP_DIR/
#./parseAndPlot.sh
#cd -
#Echo thermal throttle info
echo "thermal_throttle infos :"
cat /sys/devices/system/cpu/cpu0/thermal_throttle/*
END_TIME=$(date +%y%m%d_%H%M%S)
echo "Expe ended at $END_TIME"

