#!/bin/bash

for command in "./bmarks/mbht" 
do
	for u in '-u0' '-u10'
	do
	for i in '-i1024 -r2048' '-i128 -r256'
	do
		for core in 6 12 18 24 30 36 48
		do
			echo $command $i $u -total\=$core 
			$command $i $u -total\=$core > tmp
			if [ $? -eq 0 ] 
			then
				ii=${i//"-"/""}
				ii=${i//" "/""}
				uu=${u//"-"/""}
				echo $ii$uu
				awk -v c=$command -v core=$core -v i=$ii -v u=$uu\
					'/Throughput/ {print c,"-total=",core, i, u, $2}' tmp >> measures 
				#awk -v c=$command -v core=$core -v i=$ii -v u=$uu \
				#	'/Throughput/ {print c,"-total=",core, i, u, $2}' tmp >> measures 
			fi
		rm tmp
		done
	done
	done
done

