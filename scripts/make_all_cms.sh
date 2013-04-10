#!/bin/bash

shared="/shared/trigonak"
bank="bmarks/bank"
ht="bmarks/mbht"
ll="bmarks/mbll"

make EXTRA_DEFINES=-DNOCM clean all
cp $bank $shared/nocm/
cp $ht $shared/nocm/
cp $ll $shared/nocm/

make EXTRA_DEFINES=-DBACKOFF_RETRY clean all
cp $bank $shared/backoff/
cp $ht $shared/backoff/
cp $ll $shared/backoff/

make EXTRA_DEFINES=-DWHOLLY clean all
cp $bank $shared/wholly/
cp $ht $shared/wholly/
cp $ll $shared/wholly/

make EXTRA_DEFINES=-DGREEDY clean all
cp $bank $shared/greedy/
cp $ht $shared/greedy/
cp $ll $shared/greedy/

make EXTRA_DEFINES=-DFAIRCM clean all
cp $bank $shared/fair/
cp $ht $shared/fair/
cp $ll $shared/fair/
