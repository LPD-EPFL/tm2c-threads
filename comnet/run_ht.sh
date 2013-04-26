#!/bin/bash

data="comnet/data";

## Lazy vs Eager wlock (Fig. 4(c))
apps="fair/mbht_eager fair/mbht";
out="$data/ht.i1024.l64.m20.u30.lazy_vs_eager.dat";
params="-d2 -i1024 -r2048 -u30 -m20 -l64";
echo "# $apps  // $params" | tee -a $out;
./runrrep 5 "$apps" "$params";
./collect2.awk out.runrrep | tee -a $out;

apps="fair/mbht_eager fair/mbht";
out="$data/ht.i1024.l128.m20.u30.lazy_vs_eager.dat";
params="-d2 -i1024 -r2048 -u30 -m20 -l128";
echo "# $apps  // $params" | tee -a $out;
./runrrep 5 "$apps" "$params";
./collect2.awk out.runrrep | tee -a $out;

