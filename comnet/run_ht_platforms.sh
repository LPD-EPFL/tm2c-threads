#!/bin/bash                              
data="comnet/data";

## hash table to compare platforms (Fig. 8(d))

apps="fair/mbht";
out="$data/ht.i512.l4.u10.platforms.dat";
params="-d2 -i512 -r1024 -u10 -l4";
echo "# $apps  // $params" | tee -a $out;
./runrrep 5 "$apps" "$params";
./collect1.awk out.runrrep | tee -a $out;

out="$data/ht.i512.l16.u10.platforms.dat";
params="-d2 -i512 -r1024 -u10 -l16";
echo "# $apps  // $params" | tee -a $out;
./runrrep 5 "$apps" "$params";
./collect1.awk out.runrrep | tee -a $out;
