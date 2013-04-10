#!/bin/bash

cores=$1;
shift;
rc="rc.hosts"
app=$1;
shift;
params="$@";

cms="nocm backoff wholly greedy fair";

for cm in $cms
do
    echo "))) __________________________ $cm";
    ./run $cores $rc $cm/$app $params
done;