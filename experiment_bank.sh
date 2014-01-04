#!/bin/bash

for command in "./bmarks/bank" 
do
	for core in 6 12 18 24 30 36 48
	do
			echo $command $core
			$command -total=$core -c90 > tmp
			if [ $? -eq 0 ] 
			then
				gawk -v c=$command -v co=$core '/Throughput/ {print c,co, $2}' tmp >> measures 
			fi
	done
done
