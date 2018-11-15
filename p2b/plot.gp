#! /usr/local/cs/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#
# output:
#	lab2b_1.png ... total number operations per second vs number threads
#
#	lab2_list-2.png ... threads and iterations that run (un-protected) w/o failure
#	lab2_list-3.png ... threads and iterations that run (protected) w/o failure
#	lab2_list-4.png ... cost per operation vs number of threads
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#

# general plot parameters
set terminal png
set datafile separator ","

set title "lab2b-1: Throughput vs Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_1.png'
set key right top
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list w/ mutex' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list w/spin-lock' with linespoints lc rgb 'red'

set title "lab2b-2: Mutex-wait-time and Time Per Operation vs Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Time"
set logscale y 10
set output 'lab2b_2.png'
set key left top
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
        title 'time per operation' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
        title 'mutex-wait-time' with linespoints lc rgb 'red'

set title "lab2b-3: Iterations that run without failure vs Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Iterations"
set logscale y 10
set output 'lab2b_3.png'
set key left top
plot \
     "< grep -e 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
        title 'unprotected' with points lc rgb 'blue', \
     "< grep -e 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
        title 'mutex' with points lc rgb 'red', \
     "< grep -e 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
        title 'spin-lock' with points lc rgb 'green'

set title "lab2b-4: Throughput vs Threads (sublist, mutex)"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_4.png'
set key right top
plot \
     "< grep -E 'list-none-m,[0-9]2?,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '1 sublist' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '4 sublists' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '8 sublists' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '16 sublists' with linespoints lc rgb 'brown'

set title "lab2b-5: Throughput vs Threads (sublist, spin-lock)"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_5.png'
set key right top
plot \
     "< grep -E 'list-none-s,[0-9]2?,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '1 sublist' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '4 sublists' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '8 sublists' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '16 sublists' with linespoints lc rgb 'brown'