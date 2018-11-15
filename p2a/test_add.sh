#!/bin/sh
rm -f lab2_add.csv;\
make add;\

for i in "$@"
do
    echo "----run add test $i----"
    if [ "$i" = "1" -o "$i" = "all" ]
    then
	for nthrd in 2 4 8 12
	do
	    for niter in 10 20 40 80 100 1000 10000 100000
	    do
		./lab2_add --csv --threads=${nthrd} --iterations=${niter} --yield
	    done
	done
    fi
    if [ "$i" = "2" -o "$i" = "all" ]
    then
	for nthrd in 2 8
	do
	    for niter in 100 1000 10000 100000
	    do
		./lab2_add --csv --threads=${nthrd} --iterations=${niter}
		./lab2_add --csv --threads=${nthrd} --iterations=${niter} --yield
	    done
	done
    fi
    if [ "$i" = "3" -o "$i" = "all" ]
    then
        for niter in 10 20 40 80 100 1000 10000 100000
        do
            ./lab2_add --csv --iterations=${niter}
        done
    fi
    if [ "$i" = "4" -o "$i" = "all" ]
    then
        for nthrd in 2 4 8 12
        do
            ./lab2_add --csv --threads=${nthrd} --iterations=10000 --yield
            ./lab2_add --csv --threads=${nthrd} --iterations=10000 --yield --sync=m
            ./lab2_add --csv --threads=${nthrd} --iterations=1000  --yield --sync=s
            ./lab2_add --csv --threads=${nthrd} --iterations=10000 --yield --sync=c
        done
    fi
    if [ "$i" = "5" -o "$i" = "all" ]
    then
        for nthrd in 1 2 4 8 12
        do
            ./lab2_add --csv --threads=${nthrd} --iterations=10000
            ./lab2_add --csv --threads=${nthrd} --iterations=10000 --sync=m
            ./lab2_add --csv --threads=${nthrd} --iterations=10000 --sync=s
            ./lab2_add --csv --threads=${nthrd} --iterations=10000 --sync=c
        done
    fi
done
