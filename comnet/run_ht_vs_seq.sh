#!/bin/sh

update_rates="20 30 40 50";
load_factors="2 4 6 8";
cores="48";

duration=1;
initial=2048;
range=$((2*$initial));
params="-d$duration -i$initial -r$range ";

echo "# ht seq/tm2c with params $params";
echo "# updates $update_rates: results are seq tm2c seq tm2c ...";

for l in $load_factors
do
    printf "%-5d" $l
    for u in $update_rates
    do
	p="$params -u$u -l$l";
	res=$(./run_vs_seq.sh $cores mbht_seq backoff/mbht $p | awk '/Throughput/ { print $2,$3 }');
	printf "%-12d%-12d" $res;
    done;
    echo;
done;