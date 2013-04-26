#!/bin/bash

if [ $# -eq 1 ];
then
    platform="."$1;
fi;

data="comnet/data";

## hash table to compare platforms (Fig. 8(d))

apps="greedy/mbll";
out="$data/ll.i512.u10.platforms"$platform".dat";
params="-d2 -i512 -r1024 -u10"
echo "# $apps  // $params" | tee -a $out;
./runrrep 5 "$apps" "$params";
./collect1.awk out.runrrep | tee -a $out;

