#!/bin/bash
# EXPE=('NAS' 'param')
EXPE=('NAS')
end()
{
    echo "$$ get SIGINT"
    kill -TERM  -- -$(pgrep -o $(basename -s .sh $0))
    exit 1
}
trap end SIGINT



wait_all_pids()
{
    for pid in $@
    do
        echo "$$ waiting for child $pid"
        wait $pid
        echo "Child $pid finished"
    done
}

extract_files()
{
    cp *_extractor_$expe* $d/
    cd $d
    ./csv_extractor_$expe.sh &
    PIDS="$!"
    ./events_extractor_$expe.sh &
    PIDS="$! $PIDS"
    wait_all_pids $PIDS
    exit 0
}

parse_expe()
{
    echo "Retrieving partial results for experiment $expe"
    DIR=Global-$expe
    mkdir $DIR
    PIDS=""
    for d in $expe*
    do
        echo "Initializing files from directory $d"
        extract_files &
        PIDS="$! $PIDS"
    done
    echo "$$ have launched all children: $PIDS"

    wait_all_pids $PIDS

    init=0
    for d in $expe*
    do
        cd $d
        echo "Retreving files from directory $d"
        if [ $init -eq 0 ]
        then
            cp *.csv ../$DIR/
            cp *.rmd ../$DIR/
            init=1
        else
            sed 1d results.csv >> ../$DIR/results.csv
            sed 1d events.csv >>  ../$DIR/events.csv
        fi
        cd ..
        echo "Done"
    done
    cp analyse_$expe.rmd $DIR/analyse.rmd
    cd $DIR
    echo "generating plots"
    Rscript -e 'require(knitr); knit2html("analyse.rmd")'
    cd ..
}

for expe in ${EXPE[@]}
do
    parse_expe &
    PIDS_MAIN="$PIDS_MAIN $!"
done
wait_all_pids $PIDS_MAIN
