#!/bin/bash

data="comnet/data";
repetitions=5;

# ## CM vs. without CM (Fig. 5(a))
# apps="nocm/bank backoff/bank wholly/bank greedy/bank fair/bank";
# out="$data/bank.r20.nocm_vs_cm.dat";
# params="-d2 -r20";
# echo "# $apps // $params" | tee -a $out;
# ./runrrep $repetitions "$apps" "$params";
# ./collect5.awk out.runrrep | tee -a $out;

## Various CMs (Fig. 5(c))
apps="backoff/bank wholly/bank greedy/bank fair/bank";
out="$data/bank.R1.cms.dat";
params="-d2 -R1";
echo "# $apps // $params" | tee -a $out
./runrrep $repetitions "$apps" "$params";
./collect4.awk out.runrrep | tee -a $out;

