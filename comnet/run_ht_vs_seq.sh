#!/bin/sh

update_rates="20 30 40 50";
load_factors="2 4 6 8";
cores="48";

duration=1;
initial=4096;
range=$((2*$initial));
params="-d$duration -i$initial -r$range ";

data="comnet/data";
out="$data/ht.i$initial.d$duration.tm2c_vs_seq.dat";

echo "# ht seq/tm2c with params $params" | tee -a $out;
echo "# updates $update_rates: results are tm2c seq tm2c ..." | tee -a $out;

for l in $load_factors
do
    printf "%-5d" $l  | tee -a $out;
    for u in $update_rates
    do
	p="$params -u$u -l$l";
	res=$(./run_vs_seq.sh $cores mbht_seq fair/mbht $p | awk '/Throughput/ { print $2,$3 }');
	printf "%-12d%-12d" $res | tee -a $out;
    done;
    echo;
done;