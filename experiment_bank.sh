#!/bin/bash

for command in "./bmarks/bank" 
do
	for core in 6 12 18 24 30 36 48
	do
		for c in 0 50 90 
		do
				echo $command $core
				$command -total=$core -c$c > tmp
				if [ $? -eq 0 ] 
				then
					gawk -v command=$command -v core=$core -v c=$c '/Throughput/ {print command,"-total=" core,"-c" c, $2}' tmp >> measures 
				fi
		done
	done
done

