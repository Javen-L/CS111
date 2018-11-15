#!/bin/sh
rm -f lab2_list.csv;\
make list;\

for i in "$@"
do
    echo "----run list test $i----"
    if [ "$i" = "1" -o "$i" = "all" ]
    then
	for niter in 10 100 1000 10000 20000
	do
	    ./lab2_list --csv --iterations=${niter}
	done
    fi
    if [ "$i" = "2" -o "$i" = "all" ]
    then
	for nthrd in 2 4 8 12
	do
	    for niter in 1 10 100 1000
	    do
		./lab2_list --csv --threads=${nthrd} --iterations=${niter}
	    done
	done
	for nthrd in 2 4 8 12
        do
            for niter in 1 2 4 8 16 32
            do
                ./lab2_list --csv --threads=${nthrd} --iterations=${niter} --yield=i
                ./lab2_list --csv --threads=${nthrd} --iterations=${niter} --yield=d
                ./lab2_list --csv --threads=${nthrd} --iterations=${niter} --yield=il
                ./lab2_list --csv --threads=${nthrd} --iterations=${niter} --yield=dl
            done
        done
    fi
    if [ "$i" = "3" -o "$i" = "all" ]
    then
	for som in m s
	do
	    ./lab2_list --csv --threads=12 --iterations=32 --yield=i  --sync=${som}
            ./lab2_list --csv --threads=12 --iterations=32 --yield=d  --sync=${som}
            ./lab2_list --csv --threads=12 --iterations=32 --yield=il --sync=${som}
            ./lab2_list --csv --threads=12 --iterations=32 --yield=dl --sync=${som}
	done
    fi
    if [ "$i" = "4" -o "$i" = "all" ]
    then
        for nthrd in 1 2 4 8 12 16 24
        do
            #./lab2_list --csv --threads=${nthrd} --iterations=1000
            ./lab2_list --csv --threads=${nthrd} --iterations=1000 --sync=m
            ./lab2_list --csv --threads=${nthrd} --iterations=1000 --sync=s
        done
    fi
done
