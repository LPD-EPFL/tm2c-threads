#!/bin/bash

# run bank_spin bank_spin_opt bank_ttas bank_ttas_opt bank_ticket bank_ticket_opt bank_ssmp
# using the input arguments for 1..48 cores and collect the output on a table
#

NUM_CORES=$1;
shift;


printf "#cores   %-30s\n" "ssmp"

for n in `seq 1 1 $NUM_CORES`
do
    printf "%-9d" $n
    if [ $n -gt 1 ];
    then
	r7=`./run $n bmarks/bank_ssmp $@ | awk '/)))/ { print $2,$3,$4 }'`
	printf "%-10s%-10s%-10s\n" $r7
    else
	echo "";
    fi
done;

