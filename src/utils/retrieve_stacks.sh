#!/bin/bash

if [ -z "$2" ]
then
    echo "Usage $(basename $0) stackfile structsfile"
    echo "Append the stacks to the struct file"
    exit 1
fi

hexToDec(){
    printf "%d" "0x$1"
}

OFS=$IFS
IFS=,
sed 's/^\([^-]*\)-\(\S*\).*\[\(.*\)\]$/\3,\1,\2/' $1 | while read name begin end
do
    IFS=$OFS
    begin=$(hexToDec $begin)
    end=$(hexToDec $end)
    size=$(( $end - $begin ))
    echo "$name,$begin,$size,NA" >> $2
    IFS=,
done
IFS=$OFS
