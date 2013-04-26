#!/bin/bash

data="comnet/data";


# ## CM vs. without CM (Fig. 5(a))
# apps="nocm/bank backoff/bank wholly/bank greedy/bank fair/bank";
# echo "# $apps" | tee -a $data/bank.r20.nocm_vs_cm.dat
# ./runrrep 5 "$apps" -d2 -r20;
# ./collect5.awk out.runrrep | tee -a $data/bank.r20.nocm_vs_cm.dat

## Various CMs (Fig. 5(c))
# apps="backoff/bank wholly/bank greedy/bank fair/bank";
# echo "# $apps" | tee -a $data/bank.R1.cms.dat
# ./runrrep 5 "$apps" -d2 -R1;
# ./collect4.awk out.runrrep | tee -a $data/bank.R1.cms.dat

./runrrep 5 "$(ls bank_num_dsl/*)" "-d2" "-r00 -r20" 48;
echo "# -r00 and -r20" >> $data/bank.diff_num_dsl.dat;
cat out.runrrep | tee -a $data/bank.diff_num_dsl.dat;