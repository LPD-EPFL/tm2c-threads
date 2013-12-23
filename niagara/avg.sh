#! /bin/sh

for command in "./bmarks/bank" 
do
	echo $command
	for core in 6 12 18 24 30 36 48 
	do
			awk -v e="$command $core " 'BEGIN{sum = 0; count = 0} 
				$0 ~ e {sum += $3; count++} 
				END{if (count>0) {
				  	print e, (sum/count/1000)
				}}' $1 
	done
done

