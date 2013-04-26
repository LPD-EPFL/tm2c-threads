#!/bin/bash                              

data="comnet/data";

## hash table to compare platforms (Fig. 8(d))

apps="greedy/mbll";
out="$data/ll.i512.u10.platforms.dat";
params="-d2 -i512 -r1024 -u10"
echo "# $apps  // $params" | tee -a $out;
./runrrep 5 "$apps" "$params";
./collect1.awk out.runrrep | tee -a $out;