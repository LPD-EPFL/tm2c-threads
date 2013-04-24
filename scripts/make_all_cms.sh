#!/bin/bash

shared="/shared/trigonak"

if [ $(uname -n) = "lpd48core" ];
then
shared="."
fi;

if [ $(uname -n) = "parsasrv1.epfl.ch" ];
then
shared="."
fi;


bank="bmarks/bank"
ht="bmarks/mbht"
ll="bmarks/mbll"

echo "-------------------------------------------------------------------------------------"
echo "NOCM --------------------------------------------------------------------------------"
echo "-------------------------------------------------------------------------------------"
make -k EXTRA_DEFINES=-DNOCM clean all
cp $bank $shared/nocm/
cp $ht $shared/nocm/
cp $ll $shared/nocm/

echo "-------------------------------------------------------------------------------------"
echo "BACKOFF -----------------------------------------------------------------------------"
echo "-------------------------------------------------------------------------------------"
make -k EXTRA_DEFINES=-DBACKOFF_RETRY clean all
cp $bank $shared/backoff/
cp $ht $shared/backoff/
cp $ll $shared/backoff/

echo "-------------------------------------------------------------------------------------"
echo "WHOLLY ------------------------------------------------------------------------------"
echo "-------------------------------------------------------------------------------------"
make -k EXTRA_DEFINES=-DWHOLLY clean all
cp $bank $shared/wholly/
cp $ht $shared/wholly/
cp $ll $shared/wholly/

echo "-------------------------------------------------------------------------------------"
echo "GREEDY ------------------------------------------------------------------------------"
echo "-------------------------------------------------------------------------------------"
make -k EXTRA_DEFINES=-DGREEDY clean all
cp $bank $shared/greedy/
cp $ht $shared/greedy/
cp $ll $shared/greedy/

echo "-------------------------------------------------------------------------------------"
echo "FAIRCM-------------------------------------------------------------------------------"
echo "-------------------------------------------------------------------------------------"
make -k EXTRA_DEFINES=-DFAIRCM clean all
cp $bank $shared/fair/
cp $ht $shared/fair/
cp $ll $shared/fair/
