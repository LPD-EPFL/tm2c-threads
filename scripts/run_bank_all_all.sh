#!/bin/sh

#
# run bank_spin bank_spin_opt bank_ttas bank_ttas_opt bank_ticket bank_ticket_opt bank_ssmp on Opteron
# or run bank_spin bank_tta_ bank_ticket bank bank_ssmp
# using the the combinationf duration, num accounts, and check percentage given for 1..48 / 1..64 cores 
# and collect the output placed on properly named files, and autogenerate eps graphs
#

read -p "Enter the max number of cores: " NUM_CORES

UNAME=`uname -n`;
IS_TILERA=`locate tile-gcc`;

duration=2
accounts="1024 128 64 16";
check_perc="50 80 90";

for acc in $accounts
do
    for chk in $check_perc
    do 
	echo "bank -a$acc -c$chk"
	out_dat="scripts/bank.a"$acc"_c"$chk"_diff_locks.dat"

	./scripts/run_bank_all.sh $NUM_CORES -d$duration -a$acc -c$chk | tee $out_dat;
    done;
done;
