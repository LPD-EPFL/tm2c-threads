#!/bin/bash

data="comnet/data";

## Duration (Fig. 6(a))
app="mr";
files_folder="textfiles";
files="256M 512M 1G";
chunk="8192";

# for f in $files; 
# do
#     ff=$files_folder"/"$f;
#     params="-f$ff -c$chunk";
#     out="$data/mr.f$f.c$chunk.dat";
#     echo "# mr // $params" | tee -a $out;
#     ./runavg "Duration" 6 $app $params | tee -a $out;
# done;


## scalability (Fig. 6(b))
nue=48;
files="$files 2G";
for f in $files; 
do
    ff=$files_folder"/"$f;
    params="-f$ff";
    # params_var="-c4096 -c8192 -c16384";
    params_var="-c4096";
    out="$data/mr.f$f.tm2c_vs_seq.dat";

    # echo "# mr, different chucks on $nue cores // $params for $param_var" | tee -a $out;
    # ./runavg_param "Duration" 6 $app "$params" "$params_var" $nue | tee -a $out;
    echo "# mr sequential" | tee -a $out;
    ./runavg_param "Duration" 6 $app "$params -s" "$params_var" 2 | tee -a $out;
done;




