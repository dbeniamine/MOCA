#!/bin/bash
START_TIME=$(date +%y%m%d_%H%M%S)
CMDLINE="$0 $@"
EXP_NAME=$(basename $0 .sh)
OUTPUT="exp.log"
OWNER=dbeniamine
RUN=5
PREFIX="/home/dbeniamine"
WORKPATH="/tmp"
NAS="NPB3.3-OMP/"
MOCAPATH="Moca"
MITOSPATH="Mitos"
MEMPROFPATH="MemProf"
MEMPROFTUNPATH="MemProf"
TABARNACPATH="tabarnac"
THREADS=8
export PATH=$PATH:/opt/pin
if [[ $(hostname) =~ stremi ]]
then
    CONFIGS=('MocaPin' 'Base' 'MemProf' 'MemProfTun')
else
    CONFIGS=('MocaPin' 'Base' 'Mitos' 'Pin' 'MitosTun')
fi
declare -A TARGETS
declare -A RUN_DONE
declare -A POST_ACTIONS
declare -a RAND_RUNS
BASEDIR=$PWD

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
    echo "#### Kernel:   #########"
    uname -a
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
FIRSTRUN=$(( $FIRSTRUN + 1 ))

#Continue but change the OUTPUT
exec > >(tee $OUTPUT) 2>&1
dumpInfos

# Copy files to working dir
if [ $PREFIX != $WORKPATH ]
then
    cp -rv $PREFIX/$NAS $WORKPATH/
    cp -rv $PREFIX/$MOCAPATH $WORKPATH/
    cp -rv $PREFIX/$TABARNACPATH $WORKPATH/
    cp -rv $PREFIX/$MITOSPATH $WORKPATH/
fi

#Do the first compilation
cd $WORKPATH/$NAS
echo "########################"
echo "##### NPB ##############"
echo "########################"
echo "##### git log: #########"
git log | head
echo "########################"
echo "#### git diff: #########"
git diff
echo "########################"

make clean
make suite
#make dc CLASS=A
rm bin/*.x
cd $BASEDIR

cd $WORKPATH/$TABARNACPATH
echo "########################"
echo "##### TABARNAC #########"
echo "########################"
echo "##### git log: #########"
git log | head
echo "########################"
echo "#### git diff: #########"
git diff
echo "########################"
make clean
make
cd $BASEDIR

cd $WORKPATH/$MOCAPATH/src
echo "########################"
echo "##### Moca #############"
echo "########################"
echo "##### git log: #########"
git log | head
echo "########################"
echo "#### git diff: #########"
git diff
echo "########################"
make clean
make
cd $BASEDIR

cd $WORKPATH/$MITOSPATH
echo "########################"
echo "##### Mitos ############"
echo "########################"
echo "##### git log: #########"
git log | head
echo "########################"
echo "#### git diff: #########"
git diff
echo "########################"
cd build
rm ./*
cmake ..
make
make install
cd $BASEDIR

if [[ $(hostname) =~ stremi ]]
then
    export http_proxy="http://proxy.reims.grid5000.fr:3128"
    export https_proxy="http://proxy.reims.grid5000.fr:3128"
    export ftp_proxy="http://proxy.reims.grid5000.fr:3128"
    aptitude -y install libelf-dev libglib2.0-dev
    cp -rv $PREFIX/$MEMPROFPATH $WORKPATH/
    cd $WORKPATH/$MEMPROFPATH
    echo "########################"
    echo "##### MemProf ##########"
    echo "########################"
    echo "##### git log: #########"
    git log | head
    echo "########################"
    echo "#### git diff: #########"
    git diff
    echo "########################"
    cd module
    make clean
    make
    cd ../library
    make clean
    make
    cd parser
    make clean
    make
    cd $BASEDIR
    cp -rv $PREFIX/$MEMPROFTUNPATH $WORKPATH/
    cd $WORKPATH/$MEMPROFTUNPATH
    echo "########################"
    echo "##### MemProfTun ##########"
    echo "########################"
    echo "##### git log: #########"
    git log | head
    echo "########################"
    echo "#### git diff: #########"
    git diff
    echo "########################"
    cd module
    make clean
    make
    cd ../library
    make clean
    make
    cd parser
    make clean
    make
    cd $BASEDIR
fi

init_runs(){
    id=0
    declare -a COMBI
    for r in $(seq $FIRSTRUN $LASTRUN)
    do
        for c in ${CONFIGS[*]}
        do
            for b in $WORKPATH/$NAS/bin/*
            do
                COMBI[$id]="$c%$b"
                id=$(($id+1))
            done
        done
    done
    RAND_RUNS=$(echo "${COMBI[*]}" | tr " " "\n" | shuf | tr "\n" " ")
}

do_run()
{
    run=$1
    conf=$2
    benchname=$3
    benchname=$(basename $bench)
    # Init constants
    echo "$benchname"
    LOGDIR="$EXP_DIR/$benchname/run-$run"
	echo $LOGDIR
    mkdir -p $LOGDIR
	TARGETS=([Base]='' [MemProf]="$WORKPATH/$MEMPROFPATH/scripts/profile_app.sh" \
		[MemProfTun]="$WORKPATH/$MEMPROFTUNPATH/scripts/profile_app.sh" \
		[Moca]="$WORKPATH/$MOCAPATH/src/utils/moca -d $WORKPATH/$MOCAPATH -D $LOGDIR/Moca-$benchname -c" \
        [MocaPin]="$WORKPATH/$MOCAPATH/src/utils/moca -d $WORKPATH/$MOCAPATH -P -D $LOGDIR/MocaPin-$benchname -c" \
        [Pin]="$WORKPATH/$TABARNACPATH/tabarnac -r --" [Mitos]="mitosrun" \
        [MitosTun]="mitosrun -p 20")
        
    POST_ACTIONS=([Pin]="mv *.csv $LOGDIR/" [Mitos]="mv $BASEDIR/mitos_* $LOGDIR/Mitos" \
        [MitosTun]="mv $BASEDIR/mitos_* $LOGDIR/MitosTun"\
        [MemProf]="$WORKPATH/$MEMPROFPATH/parser/parse --stdout -d1 ibs.raw | tee $LOGDIR/MemProf.out"\
        [MemProfTun]="$WORKPATH/$MEMPROFTUNPATH/parser/parse --stdout -d1 ibs.raw | tee $LOGDIR/MemProfTun.out")
        # [MocaPin]="mv $LOGDIR/MocaPin.log $LOGDIR/MocaPin-$benchname/ mv $LOGDIR/MocaPin-$benchname/Moca-$benchname.log $LOGDIR/MocaPin.log" \
        # [Moca]="mv $LOGDIR/Moca.log $LOGDIR/Moca-$benchname/; mv $LOGDIR/Moca-$benchname/Moca-$benchname.log $LOGDIR/Moca.log" \

    # Unsure no addresse space randomization
    echo 0  > /proc/sys/kernel/randomize_va_space
    # Do experiments
    cmd="${TARGETS[$conf]} $bench"
    set -x
    export OMP_NUM_THREADS=$THREADS
    $cmd > $LOGDIR/$conf.log 2> $LOGDIR/$conf.err
    bash -c "${POST_ACTIONS[$conf]}"
    set +x
    [[ "$benchname" =~ dc ]] && rm $BASEDIR/ADC.*
}

init_runs


for comb in ${RAND_RUNS[*]}
do
    # Find actual run number
    if [ -z "${RUN_DONE[$comb]}" ]
    then
        run=$FIRSTRUN
    else
        run=$((${RUN_DONE[$comb]}+1))
    fi
    echo "RUN : $run"
    conf=$(echo $comb | cut -d  '%' -f 1)
    bench=$(echo $comb | cut -d  '%' -f 2)
    do_run $run $conf $bench
    RUN_DONE[$comb]=$run
    # Save
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
# echo "retrieving expe files"
# sudo chmod -R 777 $EXP_DIR
# chown -R $OWNER: $EXP_DIR
# su $OWNER -c "cp -ur $EXP_DIR /home/$OWNER/"
# echo "thermal_throttle infos :"
# cat /sys/devices/system/cpu/cpu0/thermal_throttle/*
# END_TIME=$(date +%y%m%d_%H%M%S)
echo "Expe ended at $END_TIME"
