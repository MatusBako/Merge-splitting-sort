#!/bin/bash

average ()
{
    count=0
    sum=0

    arr=$1

    for i in "${arr[@]}"
    do
        sum=`echo "scale=10; $sum + $i" | bc`
        count=`echo " $count + 1 " | bc`
    done
    echo "scale=10; $sum / $count" | bc
}

lengthLimit=1000000
echo -n "" > sort_times

for number_cnt in $(seq 1000 1000 $lengthLimit); 
do
	#create input
	dd if=/dev/urandom bs=1 count=${number_cnt} of=numbers >/dev/null 2>/dev/null

	timeArr=()

	for i in $(seq 1 4); 
	do
		timeArr+="`mpirun -np 10 ./mss ./numbers -t` "
	done 

	stdbuf -oL echo "$number_cnt" "`average $timeArr`" >> sort_times
	stdbuf -oL echo -en "\r$number_cnt/$lengthLimit"
done