#!/bin/bash

if [ $# -eq 0 ];
then
    echo "Usage: ./run.sh number_count process_count"
    exit 1
fi

if  [ ! `echo $1 | egrep -e "^[0-9]+$"` ] || [ $1 -eq 0 ];  # [ ! `echo "$1" | egrep -q "[0-9]+"` ]; #|| [ $1 -eq 0 ];
then
    echo "First argument is not a valid positive number. Expecting count of numbers to be sorted."
    exit 1
fi 

if [ ! `echo $2 | egrep -e "^[0-9]+$"` ] || [ $1 -eq 0 ];
then
    echo "Second argument is not a valid positive number. Expecting count of processes."
    exit 1
fi

number_cnt=$1
process_cnt=$2

if [ $( expr "${number_cnt}" "<" "${process_cnt}" ) ];
then
    echo "Count of numbers is not divisible by process count!"
    exit 1
fi

# compile
#cmake . >/dev/null
#make >/dev/null
mpic++ -std=c++11 --prefix /usr/local/share/OpenMPI -o mss ./src/mss.cpp
chmod +x mss

# create file with random bytes
dd if=/dev/urandom bs=1 count=${number_cnt} of=numbers >/dev/null 2>/dev/null

# run
mpirun --prefix /usr/local/share/OpenMPI -np ${process_cnt} ./mss ./numbers

# cleanup
rm -rf numbers mss
