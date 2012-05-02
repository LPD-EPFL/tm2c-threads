#!/bin/sh

OUTPUT=oprof

sudo opcontrol --reset
sudo opcontrol --no-vmlinux --start 
./$@
sudo opcontrol --stop
sudo opcontrol --shutdown

killall $1

if [ ! -d $OUTPUT ]; then
    mkdir $OUTPUT;
fi

FILE=$(echo $(date) | sed 's/ /_/g' | sed 's/[a-zA-Z]*_//' | sed 's/_CEST.*//');
mkdir $OUTPUT/$FILE;
opannotate --source $1 --output-dir=$OUTPUT/$FILE;
