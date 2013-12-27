#!/bin/bash

for command in "./bmarks/mbll" 
do
	for u in 0 10
	do
		for core in 6 12 18 24 30 36 48
		do
			echo $command $core
			$command -i1024 -r2048 -u$u -total\=$core  > tmp
			if [ $? -eq 0 ] 
			then
				awk -v c=$command -v co=$core -v uu=$u \
					'/Throughput/ {print c,co, "-i1024 -r2048 -u", uu, $2}'\
					tmp >> measures 
			fi
		rm tmp
		done
	done
done

