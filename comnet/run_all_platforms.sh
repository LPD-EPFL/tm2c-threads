#!/bin/bash

if [ $(uname -n) = "lpd48core" ];
then
    platform="opteron";
fi;

if [ $(uname -n) = "marc041" ];
then
    platform="scc";
fi;

./comnet/run_bank_platforms.sh $platform;
./comnet/run_ht_platforms.sh $platform;
./comnet/run_ll_platforms.sh $platform;
