#!/bin/bash

rc="rc.hosts"

if [ $(uname -n) = "lpd48core" ];
then
rc=""
fi;

cores=$1;
shift;
app=$1;
shift;
params="$@";

cms="nocm backoff wholly greedy fair";

for cm in $cms
do
    echo "))) __________________________ $cm";
    ./run $cores $rc $cm/$app $params
done;