#!/bin/bash

# rck00: [00] ))) 970032
# ))) 939015    98.92  10.650    5        (Throughput, Commit Rate, Latency)

cores=$(seq 3 3 48);
repetitions=5;
duration=2;

bank_stm="fair/bank";
bank_lck="bankseq";

params1="-r0";
params2="-R1 -r0";

echo "#cores         -r0                                             -R1 -r0";
echo "# lock                            tm2c                          lock                          tm2c";

for c in $cores;
do
    printf "%-4d" $c;
    lck1=$(./runrep_ $bank_lck $c $repetitions -d$duration -l $params1 | awk ' // { printf "%d %f %f", $2, $3, $4 }');
    printf "%-10d%-10.2f%-10.3f" $lck1;
    stm1=$(./runrep_ $bank_stm $c $repetitions -d$duration $params1 | awk ' // { printf "%d %f %f", $2, $3, $4 }');
    printf "%-10d%-10.2f%-10.3f" $stm1;

    lck2=$(./runrep_ $bank_lck $c $repetitions -d$duration -l $params2 | awk ' // { printf "%d %f %f", $2, $3, $4 }');
    printf "%-10d%-10.2f%-10.3f" $lck2;
    stm2=$(./runrep_ $bank_stm $c $repetitions -d$duration $params2 | awk ' // { printf "%d %f %f", $2, $3, $4 }');
    printf "%-10d%-10.2f%-10.3f\n" $stm2;
done; 


