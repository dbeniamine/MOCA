#!/bin/bash
echo "parsing log to create csv files"
./csv_extractor.sh
echo "extracting taces informations"
./events_extractor.sh
echo "creating plots"
Rscript -e 'require(knitr); knit2html("analyse_nas.rmd")'
