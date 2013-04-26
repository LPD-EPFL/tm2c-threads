#!/bin/bash

if [ $# -eq 1 ];
then
    platform="."$1;
fi;

data="comnet/data";

## bank to compare platforms (Fig. 8(b))
apps="fair/bank";
params="-d2 -r20";
out="$data/bank.r20.platforms"$platform".dat";
echo "# $apps // $params" | tee -a $out
./runrrep 5 "$apps" "$params" 
./collect1.awk out.runrrep | tee -a $out


params="-d2 -r0";
out="$data/bank.r0.platforms"$platform".dat";
echo "# $apps // $params" | tee -a $out
./runrrep 5 "$apps" "$params" 
./collect1.awk out.runrrep | tee -a $out


