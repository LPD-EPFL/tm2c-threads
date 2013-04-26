#!/bin/bash

data="comnet/data";

repetitions=5;

duration=10;
initial=2048;
update=20;
range=$((2*$initial));
params="-d$duration -i$initial -r$range -u$update";

out="$data/ll.i2048.u20.elastic.dat";
apps="lists/mbll_greedy lists/mbll_early lists/mbll_read";
./runrrep $repetitions "$apps" "$params";
echo "# linked list: greedy, early, read" >> $out;
echo "# $params" >> $out;
./collect.awk out.runrrep | tee -a $out;