#!/bin/sh
rm -f lab2b_list.csv;\
make;\

for i in "$@"
do
    echo "----run list test $i----"
    if [ "$i" = "1" -o "$i" = "all" ]
    then
	for nthrd in 1 2 4 8 12 16 24
	do
	    ./lab2_list --csv --threads=${nthrd} --iterations=1000 --sync=m
	    ./lab2_list --csv --threads=${nthrd} --iterations=1000 --sync=s
	done
    fi
    if [ "$i" = "2" -o "$i" = "all" ]
    then
	echo "test 2 included in 1"
    fi
    if [ "$i" = "3" -o "$i" = "all" ]
    then
	for nthrd in 1 4 8 12 16
	do
	    for niter in 1 2 4 8 16
	    do
		./lab2_list --csv --lists=4 --threads=${nthrd} --iterations=${niter} --yield=id
	    done
	done
	for nthrd in 1 4 8 12 16
        do
            for niter in 10 20 40 80
            do
                ./lab2_list --csv --lists=4 --threads=${nthrd} --iterations=${niter} --yield=id --sync=s
		./lab2_list --csv --lists=4 --threads=${nthrd} --iterations=${niter} --yield=id --sync=m
            done
        done
    fi
    if [ "$i" = "4" -o "$i" = "all" ]
    then
        for nthrd in 1 2 4 8 12
        do
	    for nls in 4 8 16
	    do
		./lab2_list --csv --lists=${nls} --threads=${nthrd} --iterations=1000 --sync=m
		./lab2_list --csv --lists=${nls} --threads=${nthrd} --iterations=1000 --sync=s
	    done
        done
    fi
done
