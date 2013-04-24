#!/bin/sh

if [ $# -lt 3 ];
then
    echo "Usage: $0 NUM_CORES APPS PARAMS";
    echo "Example: $0 \"2 3 4 5 6\" \"fair/mbht nocm/mbht\" -d1 -i1024 -r2048 -u10 -l4";
    exit;
fi;

cores="$1";
shift;
apps="$1";
shift;
params="$@"

for c in $cores
do
    echo "-- #Cores $c";
    for a in $apps
    do
	printf "%-20s" $a
	res=$(./run $c $a $params | awk '/)))/ { printf "%d %f %f", $2, $3, $4 }');
	printf "%-12d%-10.1f%-10.3f" $res;
	echo;
    done;
done;