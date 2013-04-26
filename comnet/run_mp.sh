#!/bin/sh

num_msg=10000000;
freq=2.1

out="comnet/data/mp.dat"

echo "# mp $num_msg" | tee -a $out;
echo "#c   ticks     us" | tee -a $out;

./runavg "ticks" 18 bmarks/mp $num_msg | tee mp.tmp;

awk -v fr=$freq '// { printf "%-5d%-10d%d\n", $1, $2, $2/fr}' mp.tmp | tee -a $out;

rm mp.tmp;