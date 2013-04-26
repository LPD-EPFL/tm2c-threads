#!/bin/sh

num_msg=10000000;
freq=0.533

#enable for scc800
scc800=".scc800";
freq=0.800

out="comnet/data/mp"$scc800".dat"

echo "# mp $num_msg" | tee -a $out;
echo "#c   ticks     us" | tee -a $out;

./runavg "ticks" 18 mp $num_msg | tee mp.tmp;

awk -v fr=$freq '// { printf "%-5d%-10d%d\n", $1, $2, $2/fr}' mp.tmp | tee -a $out;

rm mp.tmp;