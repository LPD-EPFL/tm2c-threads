#!/bin/bash

data="comnet/data";
repetitions=2;

## diff num of dsl cores (Fig. 5(b))
apps="$(ls bank_num_dsl/*)";
out="$data/bank.diff_num_dsl.dat";
params="-r00 -r20";
echo "# $apps // $params" | tee -a $out;
./runrrep $repetitions "$apps" "-d2" "$params" 48;
cat out.runrrep | tee -a $out